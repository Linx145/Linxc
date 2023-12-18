//#include <lexer.hpp>
#include <stdio.h>
#include <parser.hpp>
#include <ArenaAllocator.hpp>
#include <reflectC.hpp>

i32 main()
{
    ArenaAllocator arena = ArenaAllocator(&defaultAllocator);

    //CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    //"C:/Users/Linus/source/repos/Linxc/linxc-out/HelloWorld.h"
    //"C:/Users/Linus/Downloads/cimgui.h"
    //"C:/VulkanSDK/1.3.239.0/Include/vulkan/vulkan_core.h"
    string filePath = string(&arena.asAllocator, "C:/Users/Linus/Downloads/vulkan_core.h");


    printf("Program started\n");
    LinxcParser parser = LinxcParser(&arena.asAllocator);
    
    /*LinxcParsedFile file = Linxc_ReflectC(&parser.globalNamespace, &arena.asAllocator, filePath.buffer);
    for (usize i = 0; i < file.definedTypes.count; i++)
    {
        printf("C file defines type %s\n", file.definedTypes.ptr[i]->name.buffer);
        for (usize j = 0; j < file.definedTypes.ptr[i]->variables.count; j++)
        {
            string typeName = file.definedTypes.ptr[i]->variables.ptr[j].type.AsTypeReference().value.GetCName(&defaultAllocator);
            printf("   has %s %s\n", typeName.buffer, file.definedTypes.ptr[i]->variables.ptr[j].name.buffer);
            typeName.deinit();
        }
    }
    filePath.deinit();*/

    //string fileFullName = string("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc");
    //string fileIncludeName = string("HelloWorld.linxc");
    //string fileContents = io::ReadFile(fileFullName.buffer);
    //printf("Parsing file\n");
    //LinxcParsedFile* result = parser.ParseFile(fileFullName, fileIncludeName, fileContents);



    /*if (result->errors.count == 0)
    {
        printf("No Error!\n__\n");

        parser.TranspileFile(result, "C:/Users/Linus/source/repos/Linxc/linxc-out/HelloWorld.c", "C:/Users/Linus/source/repos/Linxc/linxc-out/HelloWorld.h");
        printf("File written successfully\n");

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
    fileContents.deinit();*/

    parser.AddAllFilesFromDirectory(string(&arena.asAllocator, "C:/Users/Linus/source/repos/Linxc/Tests"));
    if (!parser.Compile("C:/Users/Linus/source/repos/Linxc/linxc-out"))
    {
        parser.PrintAllErrors();
        printf("Compilation failed!\n");
    }
    else printf("Compilation success!\n");

    parser.deinit();
    printf("Program finish, freeing remaining %i allocations\n", (i32)arena.ptrs.count);
    arena.deinit();
    getchar();

    return 0;
}