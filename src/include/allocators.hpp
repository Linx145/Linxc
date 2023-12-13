#ifndef linxccallocators
#define linxccallocators

#include <Linxc.h>
#include <stdlib.h>

//an example of why we desperately need traits in Linxc

def_delegate(allocFunc, void *, void *, usize);
def_delegate(freeFunc, void, void *, void *);

struct IAllocator
{
    void* instance;
    allocFunc allocFunction;
    freeFunc freeFunction;

    inline void *Allocate(usize bytes)
    {
        return allocFunction(instance, bytes);
    }
    inline void Free(void **ptr)
    {
        freeFunction(instance, *ptr);
        *ptr = NULL;
    }

    IAllocator();
    IAllocator(void *instance, allocFunc AllocateFunc, freeFunc freeFunc);
};

extern IAllocator defaultAllocator;

void *CAllocator_Allocate(void *instance, usize bytes);
void CAllocator_Free(void *instance, void *ptr);

#endif