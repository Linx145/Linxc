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
        for (usize i = 0; i < result->ast.count; i++)
        {
            string line = result->ast.Get(i)->ToString(&defaultAllocator);
            printf("%s\n", line.buffer);
            line.deinit();
        }
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
    string testStr = string(parser.allocator, "integer: ");
    testStr.Append(18446744073709551615);
    printf("%s\n", testStr.buffer);
    printf("Program finish, freeing remainding %i allocations\n", arena.ptrs.count);
    arena.deinit();
    getchar();

    return 0;
}