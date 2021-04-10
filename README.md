# Support for LuaJIT-based processing blocks

## Purpose

This component provides support for using LuaJIT in the Pothos framework.
The LuaJIT block allows the execution of array-based LuaJIT functions inside a Pothos
topology. This block passes its input and output buffers into LuaJIT, allowing
the optimized LuaJIT code to operate directly on the Pothos-allocated buffers.

This component also adds a configuration loader that allows LuaJIT blocks to be
loaded on Pothos initialization. See the **examples** directory for instructions.

## Dependencies

* C++17 compiler
* Pothos library (0.7+)
* [LuaJIT](https://luajit.org/)
* [Sol2](https://github.com/ThePhD/sol2)

## Licensing information

This module is licensed under the MIT License. To view the full license,
view LICENSE.txt.
