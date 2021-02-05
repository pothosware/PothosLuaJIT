// Copyright (c) 2020-2021 Nicholas Corgan
// SPDX-License-Identifier: MIT

#pragma once

#include "ScopedDynLib.hpp"

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
            bool exposeSetters);

        LuaJITBlock(
            const std::vector<std::string>& inputTypes,
            const std::vector<std::string>& outputTypes,
            bool exposeSetters);
        virtual ~LuaJITBlock() = default;

        void setSource(
            const std::string& luaSource,
            const std::string& functionName);

        void setPreloadedLibraries(const std::vector<std::string>& libraries);

        void activate() override;

        void deactivate() override;

        void work() override;

    private:
        sol::state _lua;
        sol::protected_function _callBlockFcn;
        sol::protected_function _blockFcn;

        bool _functionSet;

        std::vector<std::string> _dynLibPaths;
        std::vector<ScopedDynLib::SPtr> _dynLibs;
};
