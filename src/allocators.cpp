#include <allocators.hpp>

IAllocator::IAllocator()
{
    this->instance = NULL;
    this->allocFunction = NULL;
    this->freeFunction = NULL;
}
IAllocator::IAllocator(void *instance, allocFunc AllocateFunc, freeFunc freeFunc)
{
    this->instance = instance;
    this->allocFunction = AllocateFunc;
    this->freeFunction = freeFunc;
}

IAllocator defaultAllocator = IAllocator(NULL, &CAllocator_Allocate, &CAllocator_Free);

void* CAllocator_Allocate(void* instance, usize bytes)
{
    return malloc(bytes);
}
void CAllocator_Free(void* instance, void* ptr)
{
    free(ptr);
}