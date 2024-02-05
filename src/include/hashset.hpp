#pragma once

#define HASHSET_MAX_WEIGHT 0.8f

#include "Linxc.h"
#include "vector.hpp"

namespace collections
{
    template <typename T>
    struct hashset
    {
        struct Bucket
        {
            bool initialized;
            i32 keyHash;
            collections::vector<T> entries;
        };
        def_delegate(HashFunc, u32, T);
        def_delegate(EqlFunc, bool, T, T);

        IAllocator *allocator;
        HashFunc hashFunc;
        EqlFunc eqlFunc;

        Bucket *buckets;
        usize bucketsCount;
        usize filledBuckets;
        usize Count;

        hashset()
        {
            this->allocator = NULL;
            this->hashFunc = NULL;
            this->eqlFunc = NULL;
            this->Count = 0;
            this->filledBuckets = 0;
            this->bucketsCount = 32;
            this->buckets = NULL;
        }
        hashset(IAllocator *allocator, HashFunc hashFunction, EqlFunc eqlFunc)
        {
            this->allocator = allocator;
            this->hashFunc = hashFunction;
            this->eqlFunc = eqlFunc;
            this->Count = 0;
            this->filledBuckets = 0;
            this->bucketsCount = 32;
            this->buckets = (Bucket*)allocator->Allocate(this->bucketsCount * sizeof(Bucket));
            for (usize i = 0; i < this->bucketsCount; i++)
            {
                this->buckets[i].initialized = false;
            }
        }
        void deinit()
        {
            for (usize i = 0; i < bucketsCount; i++)
            {
                if (buckets[i].initialized)
                {
                    buckets[i].entries.deinit();
                }
            }
            this->allocator->Free((void**)&buckets);
        }
        void EnsureCapacity()
        {
            //in all likelihood, we may have to fill an additional bucket
            //on adding a new item. Thus, we may have to resize the underlying buffer if the weight
            //is more than 0.75
            if (filledBuckets + 1.0f >= bucketsCount * HASHMAP_MAX_WEIGHT)
            {
                usize newSize = bucketsCount * 2;

                Bucket *newBuckets = (Bucket*)this->allocator->Allocate(newSize * sizeof(Bucket));

                for (usize i = 0; i < newSize; i++)
                {
                    newBuckets[i].initialized = false;
                }
                for (usize i = 0; i < bucketsCount; i++)
                {
                    usize newIndex = buckets[i].keyHash % newSize;
                    newBuckets[newIndex] = buckets[i];
                }

                this->allocator->Free((void**)&buckets);
                buckets = newBuckets;
                bucketsCount = newSize;
            }
        }
        bool Add(T value)
        {
            EnsureCapacity();
            u32 hash = hashFunc(value);
            usize index = hash % bucketsCount;

            if (!buckets[index].initialized)
            {
                buckets[index].initialized = true;
                buckets[index].keyHash = hash;
                buckets[index].entries = collections::vector<T>(allocator);

                filledBuckets++;
            }

            for (usize i = 0; i < buckets[index].entries.count; i++)
            {
                if (eqlFunc(*buckets[index].entries.Get(i), value))
                {
                    return false;
                }
            }
            Count++;
            buckets[index].entries.Add(value);
            return true;
        }
        bool Remove(T value)
        {
            u32 hash = hashFunc(value);
            usize index = hash % bucketsCount;

            if (buckets[index].initialized)
            {
                for (usize i = 0; i < buckets[index].entries.count; i++)
                {
                    if (eqlFunc(*buckets[index].entries.Get(i), value))
                    {
                        //buckets[index].entries.Get(i)->value.V~();
                        buckets[index].entries.RemoveAt_Swap(i);

                        return true;
                    }
                }
            }
            return false;
        }
        bool Contains(T value)
        {
            u32 hash = hashFunc(value);
            u32 index = hash % bucketsCount;

            if (!buckets[index].initialized)
            {
                return false;
            }

            if (buckets[index].entries.count > 0)
            {
                for (usize i = 0; i < buckets[index].entries.count; i++)
                {
                    if (eqlFunc(value, *buckets[index].entries.Get(i)))
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    };
}