#pragma once

#include "Linxc.h"
#include "allocators.hpp"
#include <stdlib.h>
#include "array.hpp"
#include "option.hpp"

namespace collections
{
    template <typename T>
    struct vector
    {
        def_delegate(EqlFunc, bool, T, T);

        IAllocator *allocator;
        T *ptr;
        usize capacity;
        usize count;

        vector()
        {
            allocator = NULL;
            ptr = NULL;
            capacity = 0;
            count = 0;
        }
        vector(IAllocator *myAllocator)
        {
            allocator = myAllocator;
            ptr = NULL;
            capacity = 0;
            count = 0;
        }
        vector(IAllocator *myAllocator, usize minCapacity)
        {
            this->allocator = myAllocator;
            ptr = (T*)this->allocator->Allocate(sizeof(T) * minCapacity);
            capacity = minCapacity;
            count = 0;
        }
        void deinit()
        {
            if (ptr != NULL && this->allocator != NULL)
            {
                this->allocator->Free((void**)&ptr);
            }
            count = 0;
            capacity = 0;
        }

        void EnsureArrayCapacity(usize minCapacity)
        {
            if (capacity < minCapacity)
            {
                i32 newCapacity = capacity;
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
                allocator->Free((void**)&ptr);
                ptr = newPtr;
                capacity = newCapacity;
            }
        }
        void Add(T item)
        {
            if (count >= capacity)
            {
                usize minCapacity = count;
                if (capacity + 1 > count)
                {
                    minCapacity = capacity + 1;
                }
                EnsureArrayCapacity(minCapacity);
            }
            ptr[count] = item;
            count += 1;
        }
        void Clear()
        {
            /*for (usize i = 0; i < capacity; i += 1)
            {
                ptr[i].~();
            }*/
            count = 0;
        }
        T *Get(usize index)
        {
            return &ptr[index];
        }
        void RemoveAt_Swap(usize index)
        {
            //ptr[index].~();
            if (index < count - 1)
            {
                ptr[index] = ptr[count - 1];
            }
            count -= 1;
        }
        collections::Array<T> ToOwnedArray()
        {
            if (this->ptr == NULL)
            {
                return collections::Array<T>();
            }
            T *slice = (T*)allocator->Allocate(sizeof(T) * this->count);
            for (usize i = 0; i < this->count; i++)
            {
                slice[i] = this->ptr[i];
            }
            collections::Array<T> result = collections::Array<T>(this->allocator, slice, this->count);
            deinit();
            return result;
        }
        collections::Array<T> ToOwnedArrayWith(IAllocator *newAllocator)
        {
            if (this->ptr == NULL)
            {
                return collections::Array<T>(newAllocator);
            }
            T *slice = (T*)newAllocator->Allocate(sizeof(T) * this->count);
            for (usize i = 0; i < this->count; i++)
            {
                slice[i] = this->ptr[i];
            }
            collections::Array<T> result = collections::Array<T>(newAllocator, slice, this->count);
            deinit();
            return result;
        }
        option<usize> Contains(T value, EqlFunc eqlFunc)
        {
            for (usize i = 0; i < count; i++)
            {
                if (eqlFunc(this->ptr[i], value))
                {
                    option<usize>(i);
                }
            }
            return option<usize>();
        }
        collections::Array<T> ToRefArray()
        {
            return collections::Array<T>(NULL, this->ptr, this->count);
        }
        void AddAllDeinit(collections::vector<T> *from)
        {
            for (usize i = 0; i < from->count; i++)
            {
                Add(from->ptr[i]);
            }
            //memcpy(this->ptr, from->ptr, from->count * sizeof(T));
            from->deinit();
        }
    };
}