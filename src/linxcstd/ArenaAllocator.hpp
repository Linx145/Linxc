#pragma once
#include "vector.hpp"
#include "allocators.hpp"

struct ArenaAllocator
{
    collections::vector<void*> ptrs;
    IAllocator* baseAllocator;
    IAllocator asAllocator;

    inline ArenaAllocator()
    {
        ptrs = collections::vector<void *>();
        baseAllocator = NULL;
        asAllocator = IAllocator();
    }
    inline ArenaAllocator(IAllocator *base);

    inline void deinit()
    {
        for (usize i = 0; i < this->ptrs.count; i++)
        {
            this->baseAllocator->Free(this->ptrs.Get(i));
        }
        ptrs.deinit();
    }
};

inline void* ArenaAllocator_Allocate(void* instance, usize bytes)
{
    ArenaAllocator* self = (ArenaAllocator*)instance;
    void* result = self->baseAllocator->Allocate(bytes);
    self->ptrs.Add(result);
    return result;
}
inline void ArenaAllocator_Free(void* instance, void* ptr)
{

}
ArenaAllocator::ArenaAllocator(IAllocator* base)
{
    this->ptrs = collections::vector<void*>(base);
    this->baseAllocator = base;
    this->asAllocator = IAllocator(this, &ArenaAllocator_Allocate, &ArenaAllocator_Free);
}