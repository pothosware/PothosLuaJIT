// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: MIT

#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#define SOL_USING_CXX_LUA_JIT 1

#include <Pothos/Config.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>

#include <lua.hpp>
#include <sol/sol.hpp>

#include <Poco/File.h>
#include <Poco/Logger.h>
#include <Poco/Path.h>

#include <iostream>
#include <functional>
#include <string>
#include <vector>

//
// Embedded Lua
//

static const std::string BlockEnvScript = R"(

local ffi = require("ffi")

BlockEnv = {}

function BlockEnv.CallBlockFunction(fcn, inputBuffers, outputBuffers, elems)
    local inputBuffersFFI = ffi.new("void*["..tostring(#inputBuffers).."]")
    for i = 1,#inputBuffers
    do
        inputBuffersFFI[i-1] = inputBuffers[i]
    end

    local outputBuffersFFI = ffi.new("void*["..tostring(#outputBuffers).."]")
    for i = 1,#outputBuffers
    do
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
// Interface
//

class LuaJITBlock: public Pothos::Block
{
    public:
        static Pothos::Block* make(
            const std::vector<std::string>& inputTypes,
            const std::vector<std::string>& outputTypes);

        LuaJITBlock(
            const std::vector<std::string>& inputTypes,
            const std::vector<std::string>& outputTypes);
        virtual ~LuaJITBlock() = default;

        void setSource(
            const std::string& luaSource,
            const std::string& functionName);

        void work() override;

    private:
        sol::state _lua;

        std::string _functionName;
        bool _functionSet;
};

//
// Implementation
//

Pothos::Block* LuaJITBlock::make(
    const std::vector<std::string>& inputTypes,
    const std::vector<std::string>& outputTypes)
{
    return new LuaJITBlock(inputTypes, outputTypes);
}

LuaJITBlock::LuaJITBlock(
    const std::vector<std::string>& inputTypes,
    const std::vector<std::string>& outputTypes): _lua(), _functionSet(false)
{
    _lua.open_libraries();
    _lua["BlockEnv"] = safeLuaCall(_lua.load(BlockEnvScript));

    for(size_t inputIndex = 0; inputIndex < inputTypes.size(); ++inputIndex)
    {
        this->setupInput(inputIndex, inputTypes[inputIndex]);
    }
    for(size_t outputIndex = 0; outputIndex < outputTypes.size(); ++outputIndex)
    {
        this->setupOutput(outputIndex, outputTypes[outputIndex]);
    }

    this->registerCall(this, POTHOS_FCN_TUPLE(LuaJITBlock, setSource));
}

void LuaJITBlock::setSource(
    const std::string& luaSource,
    const std::string& functionName)
{
    // If this is a path, import it as a script. Else, take it as a string literal.
    // The exists() check should theoretically take care of the case where, for
    // *some* reason, the source ends with ".lua".
    if(Poco::Path(luaSource).getExtension() == ".lua")
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
    const auto type = _lua["BlockEnv"]["UserEnv"][functionName].get_type();
    if(type == sol::type::lua_nil)
    {
        throw Pothos::InvalidArgumentException("The given field ("+functionName+")"+" does not exist.");
    }
    else if(type != sol::type::function)
    {
        const auto typeName = sol::type_name(_lua, type);
        throw Pothos::InvalidArgumentException("The given field ("+functionName+")"+" must be a function. Found "+typeName+".");
    }
    else _functionName = functionName;

    _functionSet = true;
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
        _lua["BlockEnv"]["CallBlockFunction"],
        _lua["BlockEnv"]["UserEnv"][_functionName],
        workInfo.inputPointers,
        workInfo.outputPointers,
        elems);

    for(auto* input: inputs) input->consume(elems);
    for(auto* output: outputs) output->produce(elems);
}

//
// Registration
//

static Pothos::BlockRegistry registerLuaJITBlock(
    "/blocks/luajit_block",
    &LuaJITBlock::make);
