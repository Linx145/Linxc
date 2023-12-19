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
    //string filePath = string(&arena.asAllocator, "C:/Users/Linus/Downloads/vulkan_core.h");

    printf("Program started\n");
    LinxcParser parser = LinxcParser(&arena.asAllocator);

    parser.includeDirectories.Add(string(&arena.asAllocator, "C:/Users/Linus/source/repos/Linxc/Tests"));
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