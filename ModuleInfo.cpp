// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: MIT

#include <Pothos/Plugin.hpp>

#include <json.hpp>

#include <lua.hpp>
#include <sol/sol.hpp>

#include <iostream>

using nlohmann::json;

static std::string __getPothosLuaJITInfo()
{
    sol::state lua;
    lua.open_libraries();

    json topObject;
    topObject["LuaJIT Version"] = lua["jit"]["version"].get<std::string>();

    return topObject.dump();
}

static std::string getPothosLuaJITInfo()
{
    // Only do this once.
    static const auto info = __getPothosLuaJITInfo();

    return info;
}

pothos_static_block(registerPothosLuaJITInfo)
{
    Pothos::PluginRegistry::addCall(
        "/devices/luajit/info",
        &getPothosLuaJITInfo);
}
