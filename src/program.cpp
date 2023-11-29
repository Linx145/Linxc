//#include <lexer.hpp>
#include <stdio.h>
#include <parser.hpp>

i32 main()
{
    printf("Program started\n");
    LinxcParser parser = LinxcParser(&defaultAllocator);
    //parser.includedFiles.Add(string("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc"));
    string fileFullName = string("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc");
    string fileIncludeName = string("HelloWorld.linxc");
    string fileContents = io::ReadFile(fileFullName.buffer);
    printf("Parsing file\n");
    LinxcParsedFile* result = parser.ParseFile(fileFullName, fileIncludeName, fileContents);

    if (result->errors.count == 0)
    {
        printf("No Error!\n");
    }
    else
    {
        for (usize i = 0; i < result->errors.count; i++)
        {
            printf("Error: %s\n", result->errors.Get(i)->buffer);
        }
    }

    fileFullName.deinit();
    fileIncludeName.deinit();
    fileContents.deinit();

    parser.deinit();
    printf("Program finish\n");
    getchar();

    return 0;
}