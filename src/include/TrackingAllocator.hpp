#pragma once
#include "hashset.hpp"
#include "allocators.hpp"
#include "hash.hpp"

struct TrackingAllocator
{
	IAllocator* baseAllocator;
	IAllocator asAllocator;
	collections::hashset<void*> allocated;

	TrackingAllocator(IAllocator* base)
	{
		baseAllocator = base;
		allocated = collections::hashset<void*>(baseAllocator, &PointerHash<void>, &PointerEql<void>);
		asAllocator = IAllocator(this, &TrackingAllocator_Allocate, &TrackingAllocator_Free);
	}

	void deinit()
	{
		for (usize i = 0; i < allocated.bucketsCount; i++)
		{
			if (allocated.buckets[i].initialized)
			{
				for (usize j = 0; j < allocated.buckets[i].entries.count; j++)
				{
					baseAllocator->freeFunction(baseAllocator->instance, allocated.buckets[i].entries.ptr[j]);
				}
			}
		}
		allocated.deinit();
	}
};
void* TrackingAllocator_Allocate(void* instance, usize bytes)
{
	TrackingAllocator* allocator = (TrackingAllocator*)instance;
	void* result = allocator->baseAllocator->allocFunction(allocator->baseAllocator->instance, bytes);
	if (result != NULL)
	{
		allocator->allocated.Add(result);
	}
	return result;
}
void TrackingAllocator_Free(void* instance, void* ptr)
{
	TrackingAllocator* allocator = (TrackingAllocator*)instance;
	if (allocator->allocated.Remove(ptr))
	{
		allocator->baseAllocator->freeFunction(allocator->baseAllocator->instance, ptr);
	}
}