#pragma once

#define HASHMAP_MAX_WEIGHT 0.8f

#include "Linxc.h"
#include "vector.hpp"
#include <stdio.h>

namespace collections
{
    template <typename K, typename V>
    struct hashmap
    {
        IAllocator *allocator;
        struct Entry
        {
            K key;
            V value;

            Entry(K key, V value)
            {
                this->key = key;
                this->value = value;
            }
        };
        struct Bucket
        {
            bool initialized;
            u32 keyHash;
            collections::vector<Entry> entries;

            Bucket()
            {
                initialized = false;
                keyHash = 0;
                entries = collections::vector<Entry>();
            }
            Bucket(IAllocator *allocator)
            {
                initialized = false;
                keyHash = 0;
                entries = collections::vector<Entry>(allocator);
            }
        };
        def_delegate(HashFunc, u32, K);
        def_delegate(EqlFunc, bool, K, K);

        HashFunc hashFunc;
        EqlFunc eqlFunc;

        Bucket *buckets;
        usize bucketsCount;
        usize filledBuckets;
        usize Count;

        hashmap()
        {
            this->allocator = NULL;
            this->hashFunc = NULL;
            this->eqlFunc = NULL;
            this->Count = 0;
            this->filledBuckets = 0;
            this->bucketsCount = 32;
            this->buckets = NULL;
        }
        hashmap(IAllocator *myAllocator, HashFunc hashFunction, EqlFunc eqlFunc)
        {
            this->allocator = myAllocator;
            this->hashFunc = hashFunction;
            this->eqlFunc = eqlFunc;
            this->Count = 0;
            this->filledBuckets = 0;
            this->bucketsCount = 32;
            this->buckets = (Bucket*)this->allocator->Allocate(this->bucketsCount * sizeof(Bucket));
            for (usize i = 0; i < this->bucketsCount; i++)
            {
                this->buckets[i] = Bucket(this->allocator);
            }
        }
        void deinit()
        {
            if (buckets != NULL)
            {
                for (usize i = 0; i < bucketsCount; i++)
                {
                    if (buckets[i].initialized)
                    {
                        buckets[i].entries.deinit();
                    }
                    //buckets[i].entries.~();
                }
                allocator->Free((void**)&buckets);
            }
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
                    newBuckets[i] = Bucket(this->allocator);
                }
                for (usize i = 0; i < bucketsCount; i++)
                {
                    if (buckets[i].initialized)
                    {
                        for (usize j = 0; j < buckets[i].entries.count; j++)
                        {
                            u32 newKeyHash = hashFunc(buckets[i].entries.ptr[j].key);
                            usize newIndex = newKeyHash % newSize;
                            newBuckets[newIndex].initialized = true;
                            newBuckets[newIndex].keyHash = newKeyHash;
                            newBuckets[newIndex].entries.Add(Entry(buckets[i].entries.ptr[j].key, buckets[i].entries.ptr[j].value));
                        }
                        buckets[i].entries.deinit();
                    }
                }

                this->allocator->Free((void**)&buckets);
                buckets = newBuckets;
                bucketsCount = newSize;
            }
        }

        V* Add(K key, V value)
        {
            EnsureCapacity();
            u32 hash = hashFunc(key);
            usize index = hash % bucketsCount;

            if (!buckets[index].initialized)
            {
                buckets[index].initialized = true;
                buckets[index].keyHash = hash;

                filledBuckets++;
            }
            for (usize i = 0; i < buckets[index].entries.count; i++)
            {
                if (eqlFunc(buckets[index].entries.Get(i)->key, key))
                {
                    buckets[index].entries.Get(i)->value = value;
                    return &buckets[index].entries.Get(i)->value;
                }
            }
            Count++;

            Entry newEntry = Entry(key, value);

            buckets[index].entries.Add(newEntry);
            return &buckets[index].entries.Get(buckets[index].entries.count - 1)->value;
        }

        bool Remove(K key)
        {
            u32 hash = hashFunc(key);
            usize index = hash % bucketsCount;

            if (buckets[index].initialized)
            {
                for (usize i = 0; i < buckets[index].entries.count; i++)
                {
                    if (eqlFunc(buckets[index].entries.Get(i)->key, key))
                    {
                        buckets[index].entries.RemoveAt_Swap(i);

                        return true;
                    }
                }
            }
            return false;
        }

        V *Get(K key)
        {
            u32 hash = hashFunc(key);
            usize index = hash % bucketsCount;

            if (buckets[index].initialized)
            {
                for (usize i = 0; i < buckets[index].entries.count; i++)
                {
                    if (eqlFunc(buckets[index].entries.Get(i)->key, key))
                    {
                        return &buckets[index].entries.Get(i)->value;
                    }
                }
            }
            return NULL;
        }

        V GetCopyOr(K key, V valueOnNotFound)
        {
            u32 hash = hashFunc(key);
            usize index = hash % bucketsCount;

            if (buckets[index].initialized)
            {
                for (usize i = 0; i < buckets[index].entries.count; i++)
                {
                    if (eqlFunc(buckets[index].entries.Get(i)->key, key))
                    {
                        return buckets[index].entries.Get(i)->value;
                    }
                }
            }
            return valueOnNotFound;
        }

        bool Contains(K key)
        {
            u32 hash = hashFunc(key);
            usize index = hash % bucketsCount;

            if (!buckets[index].initialized)
            {
                return false;
            }

            for (usize i = 0; i < buckets[index].entries.count; i++)
            {
                if (eqlFunc(key, buckets[index].entries.Get(i)->key))
                {
                    return true;
                }
            }
            return false;
        }

        hashmap<K, V> Clone(IAllocator *newAllocator)
        {
            hashmap<K, V> result = hashmap<K, V>(newAllocator, this->hashFunc, this->eqlFunc);

            for (usize i = 0; i < bucketsCount; i++)
            {
                if (buckets[i].initialized)
                {
                    for (usize j = 0; j < buckets[i].entries.count; j++)
                    {
                        result.Add(buckets[i].entries.Get(j)->key, buckets[i].entries.Get(j)->value);
                    }
                }
            }

            return result;
        }
    };
}