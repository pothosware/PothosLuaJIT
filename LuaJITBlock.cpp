// Copyright (c) 2020-2021 Nicholas Corgan
// SPDX-License-Identifier: MIT

#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#define SOL_USING_CXX_LUA_JIT 1

#include "LuaJITBlock.hpp"

#include <Pothos/Exception.hpp>

#include <Poco/File.h>
#include <Poco/Path.h>

#include <algorithm>
#include <string>
#include <vector>

//
// Embedded Lua
//

static const std::string BlockEnvScript = R"(

local ffi = require("ffi")

BlockEnv = {}

function BlockEnv.CallBlockFunction(fcn, inputBuffers, outputBuffers, elems)
    -- Copy pointers to FFI buffers so the block function can cast them
    -- as needed.
    local inputBuffersFFI = ffi.new("void*[?]", #inputBuffers)
    for i = 1,#inputBuffers
    do
        -- LuaJIT buffers are 0-indexed.
        inputBuffersFFI[i-1] = inputBuffers[i]
    end

    -- Copy pointers to FFI buffers so the block function can cast them
    -- as needed.
    local outputBuffersFFI = ffi.new("void*[?]", #outputBuffers)
    for i = 1,#outputBuffers
    do
        -- LuaJIT buffers are 0-indexed.
        outputBuffersFFI[i-1] = outputBuffers[i]
    end

    fcn(inputBuffersFFI, #inputBuffers, outputBuffersFFI, #outputBuffers, elems)
end

return BlockEnv

)";

//
// Utility code
//

template <typename... ArgsType>
static sol::protected_function_result safeLuaCall(const sol::protected_function& fcn, ArgsType... args)
{
    sol::protected_function_result pfr;

    try
    {
        pfr = fcn(args...);
        if(!pfr.valid())
        {
            sol::error err = pfr;
            throw Pothos::Exception(err.what());
        }
    }
    catch(const sol::error& err)
    {
        throw Pothos::Exception(err.what());
    }

    return pfr;
}

//
// Implementation
//

Pothos::Block* LuaJITBlock::make(
    const std::vector<std::string>& inputTypes,
    const std::vector<std::string>& outputTypes,
    bool exposeSetters)
{
    return new LuaJITBlock(inputTypes, outputTypes, exposeSetters);
}

LuaJITBlock::LuaJITBlock(
    const std::vector<std::string>& inputTypes,
    const std::vector<std::string>& outputTypes,
    bool exposeSetters): _lua(), _functionSet(false)
{
    _lua.open_libraries();
    _lua["BlockEnv"] = safeLuaCall(_lua.load(BlockEnvScript));
    _callBlockFcn = _lua["BlockEnv"]["CallBlockFunction"];

    for(size_t inputIndex = 0; inputIndex < inputTypes.size(); ++inputIndex)
    {
        this->setupInput(inputIndex, inputTypes[inputIndex]);
    }
    for(size_t outputIndex = 0; outputIndex < outputTypes.size(); ++outputIndex)
    {
        this->setupOutput(outputIndex, outputTypes[outputIndex]);
    }

    if(exposeSetters)
    {
        this->registerCall(this, POTHOS_FCN_TUPLE(LuaJITBlock, setSource));
        this->registerCall(this, POTHOS_FCN_TUPLE(LuaJITBlock, setPreloadedLibraries));
    }
}

void LuaJITBlock::setSource(
    const std::string& luaSource,
    const std::string& functionName)
{
    if(this->isActive())
    {
        throw Pothos::RuntimeException("Cannot set source for active block.");
    }

    // If this is a path, import it as a script. Else, take it as a string literal.
    // The exists() check should theoretically take care of the case where, for
    // *some* reason, the source ends with ".lua".
    if(Poco::Path(luaSource).getExtension() == "lua")
    {
        if(Poco::File(luaSource).exists())
        {
            _lua["BlockEnv"]["UserEnv"] = safeLuaCall(_lua.load_file(luaSource));
        }
        else throw Pothos::FileNotFoundException(luaSource);
    }
    else
    {
        _lua["BlockEnv"]["UserEnv"] = safeLuaCall(_lua.load(luaSource));
    }

    // Make sure the given entry point exists and is a function.
    sol::optional<sol::object> maybeFunc = _lua["BlockEnv"]["UserEnv"][functionName];
    if(!maybeFunc)
    {
        throw Pothos::InvalidArgumentException("The given field ("+functionName+")"+" does not exist.");
    }

    const auto type = (*maybeFunc).get_type();
    if(type != sol::type::function)
    {
        const auto typeName = sol::type_name(_lua, type);
        throw Pothos::InvalidArgumentException("The given field ("+functionName+")"+" must be a function. Found "+typeName+".");
    }
    else _blockFcn = (*maybeFunc);

    _functionSet = true;
}

void LuaJITBlock::setPreloadedLibraries(const std::vector<std::string>& libraries)
{
    if(this->isActive())
    {
        throw Pothos::RuntimeException("Cannot set preloaded libraries for active block.");
    }

    _dynLibs.clear();
    _dynLibPaths = libraries;
}

void LuaJITBlock::activate()
{
    std::transform(
        _dynLibPaths.begin(),
        _dynLibPaths.end(),
        std::back_inserter(_dynLibs),
        ScopedDynLib::load);
}

void LuaJITBlock::deactivate()
{
    _dynLibs.clear();
}

void LuaJITBlock::work()
{
    if(!_functionSet)
    {
        throw Pothos::Exception("LuaJIT function not set.");
    }

    const auto& workInfo = this->workInfo();

    const auto elems = workInfo.minElements;
    if(0 == elems) return;

    auto inputs = this->inputs();
    auto outputs = this->outputs();

    safeLuaCall(
        _callBlockFcn,
        _blockFcn,
        workInfo.inputPointers,
        workInfo.outputPointers,
        elems);

    for(auto* input: inputs) input->consume(elems);
    for(auto* output: outputs) output->produce(elems);
}

//
// Registration
//

/***********************************************************************
 * |PothosDoc LuaJIT Block
 *
 * The LuaJIT Block takes in a LuaJIT table (via script or source file)
 * containing a function to execute. This function operates directly on
 * the block's Pothos-allocated buffers.
 *
 * |category /LuaJIT
 * |keywords lua jit ffi interop
 *
 * |param inputTypes[Input Types] An array of input port types.
 * |unit bytes
 * |default ["float32"]
 *
 * |param outputTypes[Output Types] An array of output port types.
 * |unit bytes
 * |default ["float32"]
 *
 * |param source[LuaJIT Source] Source code containing the function to execute.
 * The source can either be a string returning the source code or a
 * path to a .lua file containing this source code.
 * |default ""
 * |widget FileEntry(mode=open)
 *
 * |param functionName[Function] The name of a function in the given source.
 * |default ""
 * |widget StringEntry()
 *
 * |factory /blocks/luajit_block(inputTypes,outputTypes)
 * |setter setSource(source, functionName)
 */
static Pothos::BlockRegistry registerLuaJITBlock(
    "/blocks/luajit_block",
    Pothos::Callable(&LuaJITBlock::make)
        .bind<bool>(true, 2));
