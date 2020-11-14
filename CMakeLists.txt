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

########################################################################
## Feature registration
########################################################################
include(FeatureSummary)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_LUAJIT "Enable Pothos Blocks component" ON "Pothos_FOUND;LUAJIT_FOUND;sol2_FOUND" OFF)
add_feature_info(LuaJIT ENABLE_LUAJIT "A Pothos toolkit that calls into LuaJIT code")

########################################################################
# json.hpp header
########################################################################
find_path(JSON_HPP_INCLUDE_DIR NAMES json.hpp PATH_SUFFIXES nlohmann)

if (NOT JSON_HPP_INCLUDE_DIR)
    message(STATUS "Pothos GPU toolkit requires json.hpp. Exiting...")
    return()
endif ()

########################################################################
# Build
########################################################################
set(sources
    ModuleInfo.cpp
    )

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
    SOURCES ${sources}
    LIBRARIES ${libraries}
    DESTINATION luajit
)

set_property(TARGET PothosLuaJIT PROPERTY CXX_STANDARD 17)
