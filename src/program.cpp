//#include <lexer.hpp>
#include <stdio.h>
#include <parser.hpp>
#include <ArenaAllocator.hpp>

i32 main()
{
    ArenaAllocator arena = ArenaAllocator(&defaultAllocator);

    printf("Program started\n");
    LinxcParser parser = LinxcParser(&arena.asAllocator);
    
    string fileFullName = string("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc");
    string fileIncludeName = string("HelloWorld.linxc");
    string fileContents = io::ReadFile(fileFullName.buffer);
    printf("Parsing file\n");
    LinxcParsedFile* result = parser.ParseFile(fileFullName, fileIncludeName, fileContents);

    if (result->errors.count == 0)
    {
        printf("No Error!\n__\n");

        parser.TranspileFile(result, "C:/Users/Linus/source/repos/Linxc/linxc-out/HelloWorld.c", "C:/Users/Linus/source/repos/Linxc/linxc-out/HelloWorld.h");
        printf("File written successfully\n");

        /*for (usize i = 0; i < result->ast.count; i++)
        {
            LinxcStatement* stmt = result->ast.Get(i);
            string line = stmt->ToString(&defaultAllocator);
            printf("%s\n", line.buffer);
            line.deinit();
        }*/
        printf("__\n");
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
    printf("Program finish, freeing remainding %i allocations\n", (i32)arena.ptrs.count);
    arena.deinit();
    getchar();

    return 0;
}