# Linxc
Linxc is an experimental dialect of C++ that works through transpilation to C, much like the original CFront. Any IDE that can analyze C++ can also analyze linxc. It aims to remove the need to manually create header files while adding static reflection, once-per-project template specialization and more. Moreover, it enforces a cleaner syntax that is both simpler than C++ but more modern than C, ensuring readable code even for large projects.

The language is in a pre-alpha stage and does not support all planned features yet.

## Usage
Currently, no command line interface exists to use Linxcc, the Linxc transpiler. I will update this readme when one becomes available.

## Planned Features
* Transpile Linxc to .c and .h
* Optionally output .linxci headers if building a static library, which is cross-compatible with regular C++ programs.
* Static reflection w/ database built and integrated at transpile time
* Templates that only support types as generic arguments
* Templates are specialized once per reference per entire project
* Allocator based memory management
* Optional garbage collecting allocator + GC marking function generator
* All stages of the transpiler are modifiable, allowing you to write plugins for Linxc

## Differences to C/C++
* No references, rvalues, lvalues
* No move semantics
* No typedef struct {} structname;
* No destructors, use the more versatile and less dangerous allocator system instead
* No inheritance(?)
* No unsigned, long, etc. Use i8, i16, i32, i64, u8, u16 and so on instead