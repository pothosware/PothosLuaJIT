// Copyright (c) 2020-2021 Nicholas Corgan
// SPDX-License-Identifier: MIT

#include <Pothos/Config.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>
#include <Pothos/Util/Compiler.hpp>

#include <json.hpp>

#include <Poco/Format.h>
#include <Poco/NumberFormatter.h>
#include <Poco/Path.h>
#include <Poco/TemporaryFile.h>
#include <Poco/Timestamp.h>

#include <algorithm>
#include <complex>
#include <fstream>
#include <math.h> // Use C functions in LuaJIT-exposed functions
#include <string>
#include <vector>

//
// LuaJIT test functions (must be exported for LuaJIT to use)
//

extern "C"
{
    struct PothosLuaJIT_Complex
    {
        float real;
        float imag;
    };

    void POTHOS_HELPER_DLL_EXPORT PothosLuaJIT_TestAddThreeFloatBuffers(
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

    void POTHOS_HELPER_DLL_EXPORT PothosLuaJIT_TestCombineComplex(
        const float* buffIn0,
        const float* buffIn1,
        struct PothosLuaJIT_Complex* buffOut,
        size_t elems)
    {
        for(size_t i = 0; i < elems; ++i)
        {
            buffOut[i].real = buffIn0[i];
            buffOut[i].imag = buffIn1[i];
        }
    }

    void POTHOS_HELPER_DLL_EXPORT PothosLuaJIT_TestComplexConjugate(
        const struct PothosLuaJIT_Complex* buffIn,
        struct PothosLuaJIT_Complex* buffOut,
        size_t elems)
    {
        for(size_t i = 0; i < elems; ++i)
        {
            buffOut[i].real = buffIn[i].real;
            buffOut[i].imag = -buffIn[i].imag;
        }
    }
}

static_assert(sizeof(PothosLuaJIT_Complex) == sizeof(std::complex<float>), "type size mismatch");

static const std::string TestFuncsScript = R"(

local ffi = require("ffi")
ffi.cdef[[

struct PothosLuaJIT_Complex
{
    float real;
    float imag;
};

void PothosLuaJIT_TestAddThreeFloatBuffers(
    const float* buffIn0,
    const float* buffIn1,
    const float* buffIn2,
    float* buffOut,
    size_t elems);

void PothosLuaJIT_TestCombineComplex(
    const float* buffIn0,
    const float* buffIn1,
    struct PothosLuaJIT_Complex* buffOut,
    size_t elems);

void PothosLuaJIT_TestComplexConjugate(
    const struct PothosLuaJIT_Complex* buffIn,
    struct PothosLuaJIT_Complex* buffOut,
    size_t elems);

]]

TestFuncs = {}

function TestFuncs.addFloatsC(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**", buffsIn)
    local floatBuffsOut = ffi.cast("float**", buffsOut)

    ffi.C.PothosLuaJIT_TestAddThreeFloatBuffers(
        floatBuffsIn[0],
        floatBuffsIn[1],
        floatBuffsIn[2],
        floatBuffsOut[0],
        elems)
end

function TestFuncs.addFloatsLua(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**", buffsIn)
    local floatBuffsOut = ffi.cast("float**", buffsOut)

    for i = 0, elems-1
    do
        floatBuffsOut[0][i] = floatBuffsIn[0][i] + floatBuffsIn[1][i] + floatBuffsIn[2][i]
    end
end

function TestFuncs.combineComplexC(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**", buffsIn)
    local complexBuffOut = ffi.cast("struct PothosLuaJIT_Complex*", buffsOut[0])

    ffi.C.PothosLuaJIT_TestCombineComplex(
        floatBuffsIn[0],
        floatBuffsIn[1],
        complexBuffOut,
        elems)
end

function TestFuncs.combineComplexLua(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**", buffsIn)
    local complexBuffOut = ffi.cast("struct PothosLuaJIT_Complex*", buffsOut[0])

    for i = 0, elems-1
    do
        complexBuffOut[i].real = floatBuffsIn[0][i]
        complexBuffOut[i].imag = floatBuffsIn[1][i]
    end
end

function TestFuncs.complexConjugateC(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local complexBuffIn = ffi.cast("struct PothosLuaJIT_Complex*", buffsIn[0])
    local complexBuffOut = ffi.cast("struct PothosLuaJIT_Complex*", buffsOut[0])

    ffi.C.PothosLuaJIT_TestComplexConjugate(
        complexBuffIn,
        complexBuffOut,
        elems)
end

function TestFuncs.complexConjugateLua(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local complexBuffIn = ffi.cast("struct PothosLuaJIT_Complex*", buffsIn[0])
    local complexBuffOut = ffi.cast("struct PothosLuaJIT_Complex*", buffsOut[0])

    for i = 0, elems-1
    do
        complexBuffOut[i].real = complexBuffIn[i].real
        complexBuffOut[i].imag = -complexBuffIn[i].imag
    end
end

return TestFuncs

)";

//
// Test code
//

static std::string writeToFileAndGetPath(
    const std::string& str,
    const std::string& extension)
{
    auto tempFilepath = Poco::format(
                            "%s%s.%s",
                            Poco::Path::temp(),
                            Poco::NumberFormatter::format(Poco::Timestamp().epochMicroseconds()),
                            extension);
    Poco::TemporaryFile::registerForDeletion(tempFilepath);

    std::ofstream out(tempFilepath.c_str(), std::ios::out);
    out << str;

    return tempFilepath;
}

static void testLuaJITBlocks(const std::string& luaSource)
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
    // LuaJIT blocks
    //

    auto addThreeFloatBuffersC = Pothos::BlockRegistry::make(
                                    "/blocks/luajit_block",
                                    std::vector<std::string>{"float32", "float32", "float32"},
                                    std::vector<std::string>{"float32"});
    addThreeFloatBuffersC.call(
        "setSource",
        luaSource,
        "addFloatsC");
    POTHOS_TEST_CHECKPOINT();

    auto addThreeFloatBuffersLua = Pothos::BlockRegistry::make(
                                       "/blocks/luajit_block",
                                       std::vector<std::string>{"float32", "float32", "float32"},
                                       std::vector<std::string>{"float32"});
    addThreeFloatBuffersLua.call(
        "setSource",
        luaSource,
        "addFloatsLua");
    POTHOS_TEST_CHECKPOINT();

    auto combineComplexC = Pothos::BlockRegistry::make(
                               "/blocks/luajit_block",
                               std::vector<std::string>{"float32", "float32"},
                               std::vector<std::string>{"complex_float32"});
    combineComplexC.call(
        "setSource",
        luaSource,
        "combineComplexC");
    POTHOS_TEST_CHECKPOINT();

    auto combineComplexLua = Pothos::BlockRegistry::make(
                                 "/blocks/luajit_block",
                                 std::vector<std::string>{"float32", "float32"},
                                 std::vector<std::string>{"complex_float32"});
    combineComplexLua.call(
        "setSource",
        luaSource,
        "combineComplexLua");
    POTHOS_TEST_CHECKPOINT();

    auto complexConjugateC = Pothos::BlockRegistry::make(
                                 "/blocks/luajit_block",
                                 std::vector<std::string>{"complex_float32"},
                                 std::vector<std::string>{"complex_float32"});
    complexConjugateC.call(
        "setSource",
        luaSource,
        "complexConjugateC");
    POTHOS_TEST_CHECKPOINT();

    auto complexConjugateLua = Pothos::BlockRegistry::make(
                                   "/blocks/luajit_block",
                                   std::vector<std::string>{"complex_float32"},
                                   std::vector<std::string>{"complex_float32"});
    complexConjugateLua.call(
        "setSource",
        luaSource,
        "complexConjugateLua");
    POTHOS_TEST_CHECKPOINT();

    //
    // Sinks
    //

    auto sinkAddThreeBuffersC = Pothos::BlockRegistry::make("/blocks/collector_sink", "float32");
    auto sinkAddThreeBuffersLua = Pothos::BlockRegistry::make("/blocks/collector_sink", "float32");

    auto sinkComplexConjugateC = Pothos::BlockRegistry::make("/blocks/collector_sink", "complex_float32");
    auto sinkComplexConjugateLua = Pothos::BlockRegistry::make("/blocks/collector_sink", "complex_float32");

    //
    // Run topology
    //

    {
        Pothos::Topology topology;

        for(size_t i = 0; i < numSources; ++i)
        {
            topology.connect(sources[i], 0, addThreeFloatBuffersC, i);
            topology.connect(sources[i], 0, addThreeFloatBuffersLua, i);
        }
        for(size_t i = 0; i < 2; ++i)
        {
            topology.connect(sources[i], 0, combineComplexC, i);
            topology.connect(sources[i], 0, combineComplexLua, i);
        }

        topology.connect(combineComplexC, 0, complexConjugateC, 0);
        topology.connect(combineComplexLua, 0, complexConjugateLua, 0);

        topology.connect(addThreeFloatBuffersC, 0, sinkAddThreeBuffersC, 0);
        topology.connect(addThreeFloatBuffersLua, 0, sinkAddThreeBuffersLua, 0);

        topology.connect(complexConjugateC, 0, sinkComplexConjugateC, 0);
        topology.connect(complexConjugateLua, 0, sinkComplexConjugateLua, 0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.01));
    }

    auto threeBuffersCOutput = sinkAddThreeBuffersC.call<Pothos::BufferChunk>("getBuffer");
    auto threeBuffersLuaOutput = sinkAddThreeBuffersLua.call<Pothos::BufferChunk>("getBuffer");
    auto complexConjugateCOutput = sinkComplexConjugateC.call<Pothos::BufferChunk>("getBuffer");
    auto complexConjugateLuaOutput = sinkComplexConjugateLua.call<Pothos::BufferChunk>("getBuffer");

    constexpr float epsilon = 1e-3f;

    POTHOS_TEST_GT(threeBuffersCOutput.elements(), 0ULL);
    POTHOS_TEST_EQUAL(threeBuffersCOutput.dtype, threeBuffersLuaOutput.dtype);
    POTHOS_TEST_EQUAL(threeBuffersCOutput.elements(), threeBuffersLuaOutput.elements());
    POTHOS_TEST_CLOSEA(
        threeBuffersCOutput.as<const float*>(),
        threeBuffersLuaOutput.as<const float*>(),
        epsilon,
        threeBuffersCOutput.elements());

    POTHOS_TEST_GT(complexConjugateCOutput.elements(), 0ULL);
    POTHOS_TEST_EQUAL(complexConjugateCOutput.dtype, complexConjugateLuaOutput.dtype);
    POTHOS_TEST_EQUAL(complexConjugateCOutput.elements(), complexConjugateLuaOutput.elements());
    for(size_t i = 0; i < complexConjugateCOutput.elements(); ++i)
    {
        POTHOS_TEST_CLOSE(
            complexConjugateCOutput.as<const std::complex<float>*>()[i].real(),
            complexConjugateLuaOutput.as<const std::complex<float>*>()[i].real(),
            epsilon);
    }
    for(size_t i = 0; i < complexConjugateCOutput.elements(); ++i)
    {
        POTHOS_TEST_CLOSE(
            complexConjugateCOutput.as<const std::complex<float>*>()[i].imag(),
            complexConjugateLuaOutput.as<const std::complex<float>*>()[i].imag(),
            epsilon);
    }
}

POTHOS_TEST_BLOCK("/luajit/tests", test_luajit_blocks_from_file)
{
    testLuaJITBlocks(writeToFileAndGetPath(TestFuncsScript, "lua"));
}

POTHOS_TEST_BLOCK("/luajit/tests", test_luajit_blocks_from_script)
{
    testLuaJITBlocks(TestFuncsScript);
}

//
// Testing with preloaded library
//

POTHOS_TEST_BLOCK("/luajit/tests", test_luajit_blocks_with_preloaded_library)
{
    const std::vector<std::string> librarySources =
    {
        R"(

        #include <Pothos/Config.hpp>
        #include <cmath>

        extern "C" float POTHOS_HELPER_DLL_EXPORT PothosLuaJIT_Pow(
            float base,
            float exp)
        {
            return ::powf(base, exp);
        }

        )",

        R"(

        #include <Pothos/Config.hpp>
        #include <cmath>

        extern "C" float POTHOS_HELPER_DLL_EXPORT PothosLuaJIT_Abs(float val)
        {
            return ::fabs(val);
        }

        )",

        R"(

        #include <Pothos/Config.hpp>

        extern "C" float POTHOS_HELPER_DLL_EXPORT PothosLuaJIT_Div2(float val)
        {
            return (val / 2.0f);
        }

        )"
    };

    static const std::string LuaJITBlockScript = R"(

    local ffi = require("ffi")
    ffi.cdef[[

    float PothosLuaJIT_Pow(
        float base,
        float exp);

    float PothosLuaJIT_Abs(float val);

    float PothosLuaJIT_Div2(float val);

    ]]

    local TestFuncs = {}

    function TestFuncs.blockFunc(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
        local floatBuffsIn = ffi.cast("float**", buffsIn)
        local floatBuffOut = ffi.cast("float*", buffsOut[0])

        for i = 0, (elems-1)
        do
            floatBuffOut[i] = ffi.C.PothosLuaJIT_Pow(ffi.C.PothosLuaJIT_Abs(floatBuffsIn[0][i]), ffi.C.PothosLuaJIT_Div2(floatBuffsIn[1][i]))
        end
    end

    return TestFuncs

    )";

    // Build shared libraries out of test functions and set the block
    // to load them. Otherwise, the functions the block needs won't be
    // in the global C namespace.
    auto compiler = Pothos::Util::Compiler::make();
    POTHOS_TEST_TRUE(compiler->test());

    std::vector<std::string> libraryPaths;
    std::transform(
        librarySources.begin(),
        librarySources.end(),
        std::back_inserter(libraryPaths),
        [&compiler](const std::string& source)
        {
            auto compilerArgs = Pothos::Util::CompilerArgs::defaultDevEnv();
            compilerArgs.sources.emplace_back(writeToFileAndGetPath(source, "cpp"));

            return compiler->compileCppModule(compilerArgs);
        });
    POTHOS_TEST_CHECKPOINT();

    //
    // Blocks
    //

    constexpr size_t numSources = 2;
    std::vector<Pothos::Proxy> sources;
    for(size_t i = 0; i < numSources; ++i)
    {
        nlohmann::json testPlan;
        testPlan["enableBuffers"] = true;
        testPlan["minValue"] = -5;
        testPlan["maxValue"] = 5;

        sources.emplace_back(Pothos::BlockRegistry::make("/blocks/feeder_source", "float32"));
        sources.back().call("feedTestPlan", testPlan.dump());
    }

    auto luajitBlock = Pothos::BlockRegistry::make(
                           "/blocks/luajit_block",
                           std::vector<std::string>{"float32", "float32"},
                           std::vector<std::string>{"float32"});
    luajitBlock.call(
        "setSource",
        LuaJITBlockScript,
        "blockFunc");
    luajitBlock.call("setPreloadedLibraries", libraryPaths);

    auto sink = Pothos::BlockRegistry::make("/blocks/collector_sink", "float32");

    POTHOS_TEST_CHECKPOINT();

    //
    // Test topology
    //
    {
        Pothos::Topology topology;
        topology.connect(sources[0], 0, luajitBlock, 0);
        topology.connect(sources[1], 0, luajitBlock, 1);
        topology.connect(luajitBlock, 0, sink, 0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.01));
    }

    auto output = sink.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_GT(output.elements(), 0);
}
