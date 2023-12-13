#include <ArenaAllocator.hpp>
#include <allocators.hpp>

ArenaAllocator::ArenaAllocator(IAllocator* base)
{
    this->ptrs = collections::vector<void*>();
    this->baseAllocator = base;
    this->asAllocator = IAllocator(this, ArenaAllocator_Allocate, ArenaAllocator_Free);
}

void* ArenaAllocator_Allocate(void* instance, usize bytes)
{
    ArenaAllocator* self = (ArenaAllocator*)instance;
    void* result = self->baseAllocator->Allocate(bytes);
    self->ptrs.Add(result);
    return result;
}
void ArenaAllocator_Free(void* instance, void* ptr)
{
    //do nothing
}

void ArenaAllocator::deinit()
{
    for (usize i = 0; i < this->ptrs.count; i++)
    {
        this->baseAllocator->Free(this->ptrs.Get(i));
    }
    ptrs.deinit();
}