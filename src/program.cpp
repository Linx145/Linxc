//#include <lexer.hpp>
#include <stdio.h>
#include <project.hpp>

i32 main()
{
    LinxcProject project = LinxcProject(&defaultAllocator);

    project.includedFiles.Add(string("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc"));
    project.includeDirectories.Add(string("C:/Users/Linus/source/repos/Linxc/Tests"));

    project.Build();
    project.deinit();
    // string input = io::ReadFile("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc");
    return 0;
}