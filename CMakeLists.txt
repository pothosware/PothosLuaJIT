########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 3.3)
project(PothosLua CXX)

find_package(Pothos 0.7 CONFIG REQUIRED)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake/Modules)

########################################################################
# Find dependencies
########################################################################
find_package(LuaJIT)
find_package(sol2) # Lua wrapper
find_path(JSON_HPP_INCLUDE_DIR NAMES json.hpp PATH_SUFFIXES nlohmann)

########################################################################
## Feature registration
########################################################################
include(FeatureSummary)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_LUAJIT "Enable Pothos Blocks component" ON "Pothos_FOUND;LUAJIT_FOUND;sol2_FOUND;JSON_HPP_INCLUDE_DIR" OFF)
add_feature_info(LuaJIT ENABLE_LUAJIT "A Pothos toolkit that calls into LuaJIT code")
if(NOT ENABLE_LUAJIT)
    return()
endif()

########################################################################
# Build
########################################################################
set(sources
    LuaJITBlock.cpp
    LuaJITConfLoader.cpp
    ModuleInfo.cpp
    TestLuaJITBlock.cpp)

set(includes
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JSON_HPP_INCLUDE_DIR}
    ${LUAJIT_INCLUDE_DIR})

set(libraries
    sol2::sol2
    ${LUAJIT_LIBRARIES})

include_directories(${includes})

include(PothosUtil)
POTHOS_MODULE_UTIL(
    TARGET PothosLuaJIT
    ENABLE_DOCS ON
    SOURCES ${sources}
    LIBRARIES ${libraries}
    DESTINATION blocks/luajit
)
set_target_properties(PothosLuaJIT
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON)
