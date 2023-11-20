#ifndef linxccproject
#define linxccproject

#include <string.hpp>
#include <parser.hpp>
#include <vector.linxc>
#include <lexer.hpp>
#include <io.hpp>

struct LinxcProject
{
    IAllocator *allocator;
    /// The root directories for #include statements. 
    ///In pure-linxc projects, normally is your project's
    ///src folder. May consist of include folders for C .h files as well
    ///or .linxch static libraries
    collections::vector<string> includeDirectories;
    ///The actual .linxc files to be included in the compilation.
    collections::vector<string> includedFiles;
    /// The parser to be used for this project
    LinxcParser parser;

    LinxcProject(IAllocator *allocator);

    void AddAllFilesFromDirectory(string directoryPath);

    i32 Build();

    void deinit();
};

#endif