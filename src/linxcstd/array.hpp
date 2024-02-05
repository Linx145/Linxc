#pragma once

#include "Linxc.h"
#include "allocators.hpp"
#include "option.hpp"

namespace collections
{
    template<typename T>
    struct Array
    {
        def_delegate(EqlFunc, bool, T, T);

        IAllocator *allocator;
        T *data;
        usize length;

        Array()
        {
            this->allocator = NULL;
            this->data = NULL;
            this->length = 0;
        }
        Array(IAllocator* allocator)
        {
            this->allocator = allocator;
            this->data = NULL;
            this->length = 0;
        }
        Array(IAllocator* allocator, usize itemsCount)
        {
            this->allocator = allocator;
            if (itemsCount == 0)
            {
                this->data = NULL;
            }
            else this->data = (T*)allocator->Allocate(sizeof(T) * itemsCount);
            this->length = itemsCount;
        }
        Array(IAllocator *allocator, T *data, usize itemsCount)
        {
            this->allocator = allocator;
            this->data = data;
            this->length = itemsCount;
        }
        void deinit()
        {
            if (data != NULL && allocator != NULL)
            {
                allocator->Free((void**)&data);
            }
        }
        option<usize> Contains(T value, EqlFunc eqlFunc)
        {
            for (usize i = 0; i < length; i++)
            {
                if (eqlFunc(this->data[i], value))
                {
                    return option<usize>(i);
                }
            }
            return option<usize>();
        }
        Array<T> Clone(IAllocator *allocator)
        {
            Array<T> result = Array<T>(allocator, this->length);
            for (usize i = 0; i < length; i++)
            {
                result.data[i] = this->data[i];
            }
            return result;
        }
        Array<T> CloneAdd(IAllocator* allocator, Array<T> other)
        {
            Array<T> result = Array<T>(allocator, this->length + other.length);
            for (usize i = 0; i < length; i++)
            {
                result.data[i] = this->data[i];
            }
            for (usize i = 0; i < other.length; i++)
            {
                result.data[i + this->length] = other.data[i];
            }
            return result;
        }
    };
}