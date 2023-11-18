#ifndef linxccallocators
#define linxccallocators

#include <Linxc.h>
#include <vector.linxc>
#include <stdlib.h>

def_delegate(allocFunc, void *, usize);
def_delegate(freeFunc, void, void *);

struct IAllocator
{
    allocFunc Allocate;
    freeFunc Free;
};

IAllocator CAllocator();

struct ArenaAllocator
{
    collections::vector<void *> ptrs;
    IAllocator baseAllocator;

    ArenaAllocator();

    void *Allocate(usize bytes);

    void Free(void *ptr);

    void deinit();
};

#endif