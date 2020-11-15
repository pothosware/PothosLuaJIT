// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: MIT

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>

#include <lua.hpp>
#include <sol/sol.hpp>

#include <Poco/File.h>
#include <Poco/Path.h>

#include <string>
#include <vector>

//
// Utility code
//

struct LuaJITFcnParams
{
    void** inputBuffers;
    size_t numInputs;

    void** outputBuffers;
    size_t numOutputs;

    size_t elems;
};

static const std::string BlockEnvScript = R"(

local ffi = require("ffi")
ffi.cdef[[

struct LuaJITFcnParams
{
    void** inputBuffers;
    size_t numInputs;

    void** outputBuffers;
    size_t numOutputs;

    size_t elems;
};

]]

BlockEnv = {}

function BlockEnv.CallBlockFunction(params, fcn)
    local params_ = ffi.cast("LuaJitFcnParams*", params)

    BlockEnv.BlockFunction(params_.buffsIn, params_.numInputs, params_.buffsOut, params_.numOutputs, params_.elems)
end

return BlockEnv

)";

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
    // TODO: panic and error handler
    _lua.open_libraries();
    _lua["BlockEnv"] = _lua.require_script("BlockEnv", BlockEnvScript);

    for(size_t inputIndex = 0; inputIndex < inputTypes.size(); ++inputIndex)
    {
        this->setupInput(inputIndex, inputTypes[inputIndex]);
    }
    for(size_t outputIndex = 0; outputIndex < outputTypes.size(); ++outputIndex)
    {
        this->setupOutput(outputIndex, outputTypes[outputIndex]);
    }
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
            _lua["BlockEnv"]["UserEnv"] = _lua.require_file("UserEnv", luaSource);
        }
        else throw Pothos::FileNotFoundException(luaSource);
    }
    else
    {
        _lua["BlockEnv"]["UserEnv"] = _lua.require_script("UserEnv", luaSource);
    }

    // Make sure the given entry point exists and is a function.
    const auto type = _lua["BlockEnv"]["UserEnv"][functionName].get_type();
    if(type == sol::type::lua_nil)
    {
        throw Pothos::InvalidArgumentException("The given field ("+functionName+")"+" does not exist");
    }
    else if(type != sol::type::function)
    {
        const auto typeName = sol::type_name(_lua, type);
        throw Pothos::InvalidArgumentException("The given field ("+functionName+")"+" must be a function. Found "+typeName);
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

    // Using the WorkInfo members gets ugly, so we'll just make our
    // own vector.
    std::vector<void*> inputBuffers;
    std::vector<void*> outputBuffers;

    for(auto* input: inputs)   inputBuffers.emplace_back(input->buffer());
    for(auto* output: outputs) outputBuffers.emplace_back(output->buffer());

    LuaJITFcnParams params =
    {
        inputBuffers.data(),
        inputBuffers.size(),

        outputBuffers.data(),
        outputBuffers.size(),

        elems
    };

    _lua["BlockEnv"]["UserEnv"][_functionName](params);

    for(auto* input: inputs) input->consume(elems);
    for(auto* output: outputs) output->produce(elems);
}

//
// Registration
//

static Pothos::BlockRegistry registerLuaJITBlock(
    "/blocks/luajit_block",
    &LuaJITBlock::make);
