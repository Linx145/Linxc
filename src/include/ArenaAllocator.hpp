#pragma once

#include <vector.linxc>
#include <allocators.hpp>

struct ArenaAllocator
{
    collections::vector<void*> ptrs;
    IAllocator* baseAllocator;
    IAllocator asAllocator;

    ArenaAllocator(IAllocator* base);

    void deinit();
};

void* ArenaAllocator_Allocate(void* instance, usize bytes);
void ArenaAllocator_Free(void* instance, void* ptr);