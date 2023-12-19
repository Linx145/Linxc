#pragma once

#include <Linxc.h>
#include <string.hpp>
#include <array.linxc>
#include <stdio.h>

namespace io
{
    string ReadFile(IAllocator* allocator, const char* path);

    bool FileExists(const char *path);

    bool DirectoryExists(const char* path);

    bool NewDirectory(const char* path);

    FILE* CreateDirectoriesAndFile(const char* path);

    collections::Array<string> GetFilesInDirectory(IAllocator *allocator, const char *dirPath);
}