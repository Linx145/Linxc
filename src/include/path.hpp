#ifndef linxccpath
#define linxccpath

#include <Linxc.h>
#include <string.hpp>
#include <array.linxc>
#include <option.linxc>

namespace path
{
    string SwapExtension(IAllocator* allocator, string path, const char* newExtension);
    //Including the '.'
    string GetExtension(IAllocator *allocator, string path);
}

#endif