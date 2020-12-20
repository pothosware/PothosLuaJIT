// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: MIT

#pragma once

#include <Pothos/Framework.hpp>

#include <lua.hpp>
#include <sol/sol.hpp>

#include <string>
#include <vector>

class LuaJITBlock: public Pothos::Block
{
    public:
        static Pothos::Block* make(
            const std::vector<std::string>& inputTypes,
            const std::vector<std::string>& outputTypes,
            bool exposeSetSource);

        LuaJITBlock(
            const std::vector<std::string>& inputTypes,
            const std::vector<std::string>& outputTypes,
            bool exposeSetSource);
        virtual ~LuaJITBlock() = default;

        void setSource(
            const std::string& luaSource,
            const std::string& functionName);

        void work() override;

    private:
        sol::state _lua;
        sol::protected_function _callBlockFcn;
        sol::protected_function _blockFcn;

        bool _functionSet;
};
