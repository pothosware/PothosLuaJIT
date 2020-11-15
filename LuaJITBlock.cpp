// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: MIT

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>

#include <lua.hpp>
#include <sol/sol.hpp>

#include <string>
#include <vector>

//
// Utility code
//

struct LuaJITFcnParams
{
    std::vector<void*> inputBuffers;
    std::vector<void*> outputBuffers;
    size_t elems;
};

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
        sol::function _function;

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
    const std::vector<std::string>& outputTypes): _lua(), _function(), _functionSet(false)
{
    _lua.open_libraries();
    _lua.new_usertype<LuaJITFcnParams>(
        "LuaJITFcnParams",
        sol::constructors<LuaJITFcnParams()>(),
        "inputBuffers", sol::property(&LuaJITFcnParams::inputBuffers),
        "outputBuffers", sol::property(&LuaJITFcnParams::outputBuffers),
        "elems", sol::property(&LuaJITFcnParams::elems));

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
    (void)luaSource;
    (void)functionName;

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

    LuaJITFcnParams params;
    for(auto* input: inputs)   params.inputBuffers.emplace_back(input->buffer());
    for(auto* output: outputs) params.outputBuffers.emplace_back(output->buffer());
    params.elems = elems;

    for(auto* input: inputs) input->consume(elems);
    for(auto* output: outputs) output->produce(elems);
}

//
// Registration
//

static Pothos::BlockRegistry registerLuaJITBlock(
    "/blocks/luajit_block",
    &LuaJITBlock::make);
