#include <allocators.hpp>

IAllocator::IAllocator()
{
    this->Allocate = NULL;
    this->Free = NULL;
}
IAllocator::IAllocator(allocFunc AllocateFunc, freeFunc freeFunc)
{
    this->Allocate = AllocateFunc;
    this->Free = freeFunc;
}

IAllocator defaultAllocator = CAllocator();

IAllocator CAllocator()
{
    IAllocator allocator = IAllocator(&malloc, &free);
    return allocator;
}

// ArenaAllocator::ArenaAllocator()
// {
//     this->ptrs = collections::vector<void *>();
//     this->baseAllocator.Allocate = NULL;
//     this->baseAllocator.Free = NULL;
// }

// void *ArenaAllocator::Allocate(usize bytes)
// {
//     if (this->ptrs.ptr == NULL)
//     {
//         this->ptrs = collections::vector<void *>();
//     }

//     void *result;
//     if (this->baseAllocator.Allocate == NULL)
//     {
//         result = malloc(bytes);
//     }
//     else
//         this->baseAllocator.Allocate(bytes);

//     this->ptrs.Add(result);

//     return result;
// }

// void ArenaAllocator::deinit()
// {
//     if (this->baseAllocator.Free == NULL)
//     {
//         for (usize i = 0; i < this->ptrs.count; i++)
//         {
//             free(*this->ptrs.Get(i));
//         }
//     }
//     else
//     {
//         for (usize i = 0; i < this->ptrs.count; i++)
//         {
//             this->baseAllocator.Free(*this->ptrs.Get(i));
//         }
//     }
//     ptrs.deinit();
// }