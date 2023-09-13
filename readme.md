# Linxc
Linxc is a experimental language that is a superset of C, and works through transpilation to C++. It aims to solve some problems I had when writing C++ for Somnium 2, including the need to write header files in 2023, the lack of static reflection and more.

All .linxc files are also valid C/C++ files, and thus can be highlighted and analyzed by any IDE that supports C/C++. In fact, keywords in linxc are just C macros.

## Usage
After building with zig (zig build), cd into zig-out/bin and run the program with a .linxc file as first argument. 

You can specify an output file path + name (without the extension) as the second argument.

## Features
Currently Supported:
* Basic linxc transpilation to .cpp and .h
* structs: methods, fields
* functions

Planned:
* Constructors
* Static reflection
* Automatic deep copy function generation
* Some kind of file caching and batch building
* Allocators (Arena allocator)
* Templates
* Modify the transpiler to add your own keywords
* Modify the transpiler to interpret tags

Maybe:
* Destructors
* Inheritance of some sorts (Traits?)
* Translate C# to Linxc w/ GC allocator, then to C/C++