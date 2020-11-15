// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: MIT

#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

#include <json.hpp>

#include <Poco/Format.h>
#include <Poco/NumberFormatter.h>
#include <Poco/Path.h>
#include <Poco/TemporaryFile.h>
#include <Poco/Timestamp.h>

#include <fstream>
#include <string>
#include <vector>

//
// LuaJIT test functions
//

extern "C"
{
    void addThreeFloatBuffersC(
        const float* buffIn0,
        const float* buffIn1,
        const float* buffIn2,
        float* buffOut,
        size_t elems)
    {
        for(size_t i = 0; i < elems; ++i)
        {
            buffOut[i] = buffIn0[i] + buffIn1[i] + buffIn2[i];
        }
    }
}

// TODO: FFI complex numbers
static const std::string TestFuncsScript = R"(

local ffi = require("ffi")
ffi.cdef[[

void addThreeFloatBuffers(
    const float* buffIn0,
    const float* buffIn1,
    const float* buffIn2,
    float* buffOut,
    size_t elems);

]]

TestFuncs = {}

function TestFuncs.addFloatsC(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**")
    local floatBuffsOut = ffi.cast("float**")

    ffi.C.addThreeFloatBuffers(
        floatBuffsIn[0],
        floatBuffsIn[1],
        floatBuffsIn[2],
        floatBuffsOut[0],
        elems)
end

function TestFuncs.addFloatsLua(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**")
    local floatBuffsOut = ffi.cast("float**")

    for i = 1,elems,1
    do
        floatBuffsOut[1][i] = floatBuffsIn[1][i] + floatBuffsIn[2][i] + floatBuffsIn[2][i]
    end
end

return TestFuncs

)";

//
// Test code
//

/*
static std::string writeToFileAndGetPath(const std::string& str)
{
    auto tempFilepath = Poco::format(
                            "%s%c%s.lua",
                            Poco::Path::temp(),
                            Poco::Path::separator(),
                            Poco::NumberFormatter::format(Poco::Timestamp().epochMicroseconds()));
    Poco::TemporaryFile::registerForDeletion(tempFilepath);

    std::ofstream out(tempFilepath.c_str(), std::ios::out);
    out << str;

    return tempFilepath;
}
*/

// TODO: test loading from file
POTHOS_TEST_BLOCK("/luajit/tests", test_luajit_block)
{
    //
    // Sources
    //

    nlohmann::json testPlan;
    testPlan["enableBuffers"] = true;
    testPlan["minValue"] = -100;
    testPlan["maxValue"] = 100;

    constexpr size_t numSources = 3;
    std::vector<Pothos::Proxy> sources;
    for(size_t i = 0; i < numSources; ++i)
    {
        sources.emplace_back(Pothos::BlockRegistry::make("/blocks/feeder_source", "float32"));
        sources.back().call("feedTestPlan", testPlan.dump());
    }

    //
    // Luajit blocks
    //

    auto addThreeFloatBuffersC = Pothos::BlockRegistry::make(
                                    "/blocks/luajit_block",
                                    std::vector<std::string>{"float32", "float32", "float32"},
                                    std::vector<std::string>{"float32"});
    addThreeFloatBuffersC.call(
        "setSource",
        TestFuncsScript,
        "TestFuncs",
        "addFloatsC");
    POTHOS_TEST_CHECKPOINT();

    auto addThreeFloatBuffersLua = Pothos::BlockRegistry::make(
                                       "/blocks/luajit_block",
                                       std::vector<std::string>{"float32", "float32", "float32"},
                                       std::vector<std::string>{"float32"});
    addThreeFloatBuffersLua.call(
        "setSource",
        TestFuncsScript,
        "TestFuncs",
        "addFloatsLua");
    POTHOS_TEST_CHECKPOINT();

    //
    // Sinks
    //

    auto sinkThreeBuffersC = Pothos::BlockRegistry::make("/blocks/collector_sink", "float32");
    auto sinkThreeBuffersLua = Pothos::BlockRegistry::make("/blocks/collector_sink", "float32");

    //
    // Run topology
    //

    {
        Pothos::Topology topology;

        for(size_t i = 0; i < numSources; ++i)
        {
            topology.connect(sources[0], 0, addThreeFloatBuffersC, i);
            topology.connect(sources[0], 0, addThreeFloatBuffersLua, i);
        }

        topology.connect(addThreeFloatBuffersC, 0, sinkThreeBuffersC, 0);
        topology.connect(addThreeFloatBuffersLua, 0, sinkThreeBuffersLua, 0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.01));
    }

    auto threeBuffersCOutput = sinkThreeBuffersC.call<Pothos::BufferChunk>("getBuffer");
    auto threeBuffersLuaOutput = sinkThreeBuffersLua.call<Pothos::BufferChunk>("getBuffer");

    POTHOS_TEST_GT(threeBuffersCOutput.elements(), 0ULL);
    POTHOS_TEST_EQUAL(threeBuffersCOutput.dtype, threeBuffersLuaOutput.dtype);
    POTHOS_TEST_EQUAL(threeBuffersCOutput.elements(), threeBuffersLuaOutput.elements());
    POTHOS_TEST_EQUALA(
        threeBuffersCOutput.as<const float*>(),
        threeBuffersLuaOutput.as<const float*>(),
        threeBuffersCOutput.elements());
}
