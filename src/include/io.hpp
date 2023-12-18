#pragma once

#include <Linxc.h>
#include <string.hpp>
#include <array.linxc>

namespace io
{
    string ReadFile(const char *path);

    bool FileExists(const char *path);

    collections::Array<string> GetFilesInDirectory(IAllocator *allocator, const char *dirPath);
}