-- Copyright (c) 2020 Nicholas Corgan
-- SPDX-License-Identifier: MIT

local ffi = require("ffi")

ffi.cdef[[

double pow(double base, double exponent);
double sqrt(double arg);
double hypot(double x, double y);

]]

ExampleFuncs = {}

--[[
/*
|PothosDoc Cube (LuaJIT)

A basic example block that uses LuaJIT to find the cube of all inputs.

|category /LuaJIT/Examples
|keywords example ffi

|factory /luajit/examples/cube()
*/
--]]
function ExampleFuncs.cube(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local doubleBuffIn = ffi.cast("double*", buffsIn[0])
    local doubleBuffOut = ffi.cast("double*", buffsOut[0])

    -- Unlike stock Lua, LuaJIT buffers are 0-indexed.
    for i = 0, elems-1
    do
        doubleBuffOut[i] = ffi.C.pow(doubleBuffIn[i], 3.0)
    end
end

--[[
/*
|PothosDoc Square Root (LuaJIT)

A basic example block that uses LuaJIT to find the square root of all inputs.

|category /LuaJIT/Examples
|keywords example ffi

|factory /luajit/examples/sqrt()
*/
--]]
function ExampleFuncs.sqrt(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local doubleBuffIn = ffi.cast("double*", buffsIn[0])
    local doubleBuffOut = ffi.cast("double*", buffsOut[0])

    -- Unlike stock Lua, LuaJIT buffers are 0-indexed.
    for i = 0, elems-1
    do
        doubleBuffOut[i] = ffi.C.sqrt(doubleBuffIn[i])
    end
end

--[[
/*
|PothosDoc Hypotenuse (LuaJIT)

A basic example block that uses LuaJIT to find the hypotenuse of all inputs
in two buffers.

|category /LuaJIT/Examples
|keywords example ffi

|factory /luajit/examples/hypot()
*/
--]]
function ExampleFuncs.hypot(buffsIn, numBuffsIn, buffsOut, numBuffsOut, elems)
    local doubleBuffsIn = ffi.cast("double**", buffsIn)
    local doubleBuffOut = ffi.cast("double*", buffsOut[0])

    -- Unlike stock Lua, LuaJIT buffers are 0-indexed.
    for i = 0, elems-1
    do
        doubleBuffOut[i] = ffi.C.hypot(doubleBuffsIn[0][i], doubleBuffsIn[1][i])
    end
end

return ExampleFuncs
