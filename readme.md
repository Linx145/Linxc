# Linxc
Linxc is an experimental language that works through transpilation to C++. It is unique as it's syntax is a subset of C++: Any IDE that can analyze C++ can also analyze linxc. It aims to modernize C and make it tolerable to write in the year 2023 and beyond, removing the need to manually create header files, adding static reflection, traits and more while keeping things simpler than C++.

The language is in a very early alpha stage and does not support all planned features yet.

## Usage
After building with zig (zig build), cd into zig-out/bin and run the program with a folder path as input, and another folder path for outputting the transpiled C++ files.

To write linxc, you must include the Linxc.h file in your program, as linxc does not recognise C primitive types like int, long, long long, etc. Instead, it follows rust/zig primitive naming conventions, such as i8, i32, u64, usize etc. Float, double, char and bool (but not _Bool) remain in place.

As the language is incredibly WIP, there is currently no comprehensive documentation of changes between linxc and C/C++/

## Features
Currently Supported:
* Basic linxc transpilation to .cpp and .h
* structs: methods, fields
* functions
* static variables (+in struct initialization!)

WIP:
* Transpile-time specialized templates

Planned:
* Traits
* Constructors
* Static reflection
* Automatic deep copy function generation
* Some kind of file caching and batch building
* Allocators (Arena allocator)
* Modify the transpiler to interpret tags
* Operator overloading

Maybe:
* Destructors (would not be possible for a future pure C transpilation, may be implemented as a trait instead)
* Transpilation to pure C