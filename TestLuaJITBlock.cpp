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
#include <Poco/Random.h>
#include <Poco/TemporaryFile.h>
#include <Poco/Timestamp.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <string>
#include <vector>

//
// Utility functions
//

static constexpr size_t numElements = 2048;

static Pothos::BufferChunk getRandomInputs()
{
    static Poco::Random rng;

    Pothos::BufferChunk output("float32", numElements);
    for(size_t elem = 0; elem < numElements; ++elem)
    {
        // nextFloat() returns a value in the range [0,1]. This
        // places the output in the desired range of [-5,5].
        output.as<float*>()[elem] = (rng.nextFloat() * 10.0f) - 5.0f;
    }

    return output;
}

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

function TestFuncs.addFloats(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**", buffsIn)
    local floatBuffsOut = ffi.cast("float**", buffsOut)

    ffi.C.PothosLuaJIT_TestAddThreeFloatBuffers(
        floatBuffsIn[0],
        floatBuffsIn[1],
        floatBuffsIn[2],
        floatBuffsOut[0],
        elems)
end

function TestFuncs.combineComplex(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local floatBuffsIn = ffi.cast("float**", buffsIn)
    local complexBuffOut = ffi.cast("struct PothosLuaJIT_Complex*", buffsOut[0])

    ffi.C.PothosLuaJIT_TestCombineComplex(
        floatBuffsIn[0],
        floatBuffsIn[1],
        complexBuffOut,
        elems)
end

function TestFuncs.complexConjugate(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local complexBuffIn = ffi.cast("struct PothosLuaJIT_Complex*", buffsIn[0])
    local complexBuffOut = ffi.cast("struct PothosLuaJIT_Complex*", buffsOut[0])

    ffi.C.PothosLuaJIT_TestComplexConjugate(
        complexBuffIn,
        complexBuffOut,
        elems)
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
    // Generate inputs and expected outputs
    //

    constexpr size_t numSources = 3;
    std::vector<Pothos::BufferChunk> inputs(numSources);
    for(size_t i = 0; i < numSources; ++i)
    {
        inputs[i] = getRandomInputs();
    }

    Pothos::BufferChunk expectedAddFloatsOutput("float32", numElements);
    Pothos::BufferChunk expectedComplexConjugateOutput("complex_float32", numElements);
    for(size_t elem = 0; elem < numElements; ++elem)
    {
        expectedAddFloatsOutput.as<float*>()[elem] =
            inputs[0].as<const float*>()[elem] +
            inputs[1].as<const float*>()[elem] +
            inputs[2].as<const float*>()[elem];

        expectedComplexConjugateOutput.as<std::complex<float>*>()[elem] = std::conj(
            std::complex<float>(
                inputs[0].as<const float*>()[elem],
                inputs[1].as<const float*>()[elem]));
    }

    //
    // Sources
    //

    std::vector<Pothos::Proxy> sources(numSources);
    for(size_t i = 0; i < numSources; ++i)
    {
        sources[i] = Pothos::BlockRegistry::make("/blocks/feeder_source", "float32");
        sources[i].call("feedBuffer", inputs[i]);
    }

    //
    // LuaJIT blocks
    //

    auto luajitAddFloats = Pothos::BlockRegistry::make(
                               "/blocks/luajit_block",
                               std::vector<std::string>{"float32", "float32", "float32"},
                               std::vector<std::string>{"float32"});
    luajitAddFloats.call(
        "setSource",
        luaSource,
        "addFloats");
    POTHOS_TEST_CHECKPOINT();

    auto luajitCombineComplex = Pothos::BlockRegistry::make(
                                    "/blocks/luajit_block",
                                    std::vector<std::string>{"float32", "float32"},
                                    std::vector<std::string>{"complex_float32"});
    luajitCombineComplex.call(
        "setSource",
        luaSource,
        "combineComplex");
    POTHOS_TEST_CHECKPOINT();

    auto luajitComplexConjugate = Pothos::BlockRegistry::make(
                                      "/blocks/luajit_block",
                                      std::vector<std::string>{"complex_float32"},
                                      std::vector<std::string>{"complex_float32"});
    luajitComplexConjugate.call(
        "setSource",
        luaSource,
        "complexConjugate");
    POTHOS_TEST_CHECKPOINT();

    //
    // Sinks
    //

    auto addFloatsSink = Pothos::BlockRegistry::make("/blocks/collector_sink", "float32");
    auto complexConjugateSink = Pothos::BlockRegistry::make("/blocks/collector_sink", "complex_float32");

    //
    // Run topology
    //

    {
        Pothos::Topology topology;

        for(size_t i = 0; i < numSources; ++i)
        {
            topology.connect(sources[i], 0, luajitAddFloats, i);
        }
        for(size_t i = 0; i < 2; ++i)
        {
            topology.connect(sources[i], 0, luajitCombineComplex, i);
        }

        topology.connect(
            luajitAddFloats,
            0,
            addFloatsSink,
            0);

        topology.connect(
            luajitCombineComplex,
            0,
            luajitComplexConjugate,
            0);
        topology.connect(
            luajitComplexConjugate,
            0,
            complexConjugateSink,
            0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.01));
    }

    //
    // Test against expected output
    //

    constexpr float epsilon = 1e-6f;

    auto addFloatsOutput = addFloatsSink.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_EQUAL(expectedAddFloatsOutput.dtype, addFloatsOutput.dtype);
    POTHOS_TEST_EQUAL(expectedAddFloatsOutput.elements(), addFloatsOutput.elements());
    POTHOS_TEST_CLOSEA(
        expectedAddFloatsOutput.as<const float*>(),
        addFloatsOutput.as<const float*>(),
        epsilon,
        addFloatsOutput.elements());

    auto complexConjugateOutput = complexConjugateSink.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_EQUAL(expectedComplexConjugateOutput.dtype, complexConjugateOutput.dtype);
    POTHOS_TEST_EQUAL(expectedComplexConjugateOutput.elements(), complexConjugateOutput.elements());
    POTHOS_TEST_CLOSEA(
        expectedComplexConjugateOutput.as<const float*>(),
        complexConjugateOutput.as<const float*>(),
        epsilon,
        (complexConjugateOutput.elements() * 2));
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
// Testing with preloaded libraries generated at test time
//

POTHOS_TEST_BLOCK("/luajit/tests", test_luajit_blocks_with_preloaded_libraries)
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
    // Generate inputs and expected outputs
    //

    constexpr size_t numSources = 2;
    std::vector<Pothos::BufferChunk> inputs(numSources);
    for(size_t i = 0; i < numSources; ++i)
    {
        inputs[i] = getRandomInputs();
    }

    Pothos::BufferChunk expectedOutput("float32", numElements);
    for(size_t elem = 0; elem < numElements; ++elem)
    {
        expectedOutput.as<float*>()[elem] = std::pow(
            std::abs(inputs[0].as<const float*>()[elem]),
            (inputs[1].as<const float*>()[elem] / 2.0f));
    }

    //
    // Blocks
    //

    std::vector<Pothos::Proxy> sources(numSources);
    for(size_t i = 0; i < numSources; ++i)
    {
        sources[i] = Pothos::BlockRegistry::make("/blocks/feeder_source", "float32");
        sources[i].call("feedBuffer", inputs[i]);
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

    //
    // Test against expected output
    //

    constexpr float epsilon = 1e-6f;

    auto output = sink.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_EQUAL(expectedOutput.dtype, output.dtype);
    POTHOS_TEST_EQUAL(expectedOutput.elements(), output.elements());
    POTHOS_TEST_CLOSEA(
        expectedOutput.as<const float*>(),
        output.as<const float*>(),
        epsilon,
        numElements);
}
