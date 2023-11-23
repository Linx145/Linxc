#ifndef linxccpath
#define linxccpath

#include <Linxc.h>
#include <string.hpp>
#include <array.linxc>
#include <option.linxc>

namespace path
{
    //Including the '.'
    string GetExtension(IAllocator *allocator, string path);
}

#endif