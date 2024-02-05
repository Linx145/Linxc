#pragma once
#include "Linxc.h"
#include "allocators.hpp"
#include "stdio.h"

namespace collections
{
    template <typename T>
    struct sparseset
    {
        IAllocator *allocator;
        T *ptr;
        usize capacity;

        sparseset()
        {
            allocator = NULL;
            ptr = NULL;
            capacity = 0;
        }
        sparseset(IAllocator *myAllocator)
        {
            allocator = myAllocator;
            ptr = NULL;
            capacity = 0;
        }
        sparseset(IAllocator *myAllocator, usize minCapacity)
        {
            this->allocator = myAllocator;
            ptr = NULL;//(T*)this->allocator->Allocate(sizeof(T) * minCapacity);
            capacity = 0;
            //capacity = minCapacity;
            EnsureArrayCapacity(minCapacity);
        }
        void deinit()
        {
            if (ptr != NULL && this->allocator != NULL)
            {
                this->allocator->Free((void**)&ptr);
            }
            capacity = 0;
        }
        void EnsureArrayCapacity(usize minCapacity)
        {
            if (capacity < minCapacity || ptr == NULL)
            {
                usize newCapacity = capacity;
                if (newCapacity == 0)
                {
                    newCapacity = 4;
                }
                while (newCapacity < minCapacity)
                {
                    newCapacity *= 2;
                }
                T *newPtr = (T*)allocator->Allocate(sizeof(T) * newCapacity);
                for (usize i = 0; i < capacity; i += 1)
                {
                    newPtr[i] = ptr[i];
                }
                for (usize i = capacity; i < newCapacity; i++)
                {
                    newPtr[i] = {};
                }
                if (ptr != NULL)
                {
                    allocator->Free((void **)&ptr);
                }
                ptr = newPtr;
                capacity = newCapacity;
            }
        }
        void Insert(usize index, T value)
        {
            EnsureArrayCapacity(index);
            ptr[index] = value;
        }
        T *Get(usize index)
        {
            if (index >= capacity)
            {
                return NULL;
            }
            return &ptr[index];
        }
    };
}
