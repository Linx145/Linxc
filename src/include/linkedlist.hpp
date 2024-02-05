#ifndef linxclinkedlist
#define linxclinkedlist

#include "allocators.hpp"

namespace collections
{
	template<typename T>
	struct linkedlist;
	template<typename T>
	struct linkednode;

	template<typename T>
	struct linkednode
	{
		linkednode<T>* next;
		linkednode<T>* prev;
		linkedlist* list;
		T value;
	};

	template<typename T>
	struct linkedlist
	{
		IAllocator* allocator;
		linkednode<T>* first;
		linkednode<T>* last;
		usize count;

		linkedlist()
		{
			allocator = NULL;
			first = NULL;
			last = NULL;
			count = 0;
		}
		linkedlist(IAllocator* myAllocator)
		{
			allocator = myAllocator;
			first = NULL;
			last = NULL;
			count = 0;
		}
		bool AddListAfter(linkednode<T>* after, linkedlist<T>* listToAdd)
		{
			//operation can only continue if listToAdd has objects within, and the allocator is the same instance
			if (listToAdd->allocator != this->allocator || listToAdd->first == NULL || listToAdd->last == NULL)
			{
				return false;
			}
			//transfer list ownership
			linkednode<T>* node = listToAdd->first;
			while (node != NULL)
			{
				node->list = this;
				node = node->next;
			}

			linkednode<T>* originalNext = after->next;
			after->next = listToAdd->first;
			listToAdd->first->prev = after;
			originalNext->prev = listToAdd->last;
			listToAdd->last->next = originalNext;
			this->count += listToAdd->count;

			listToAdd->count = 0;
			listToAdd->first = NULL;
			listToAdd->last = NULL;
			return true;
		}
		linkednode<T>* AddBefore(T item, linkednode<T>* before)
		{
			linkednode<T>* node = (linkednode<T>*)allocator->Allocate(sizeof(linkednode<T>));
			node->list = this;
			node->value = item;
			linkednode<T>* originalPrev = before->prev;
			before->prev = node;
			node->next = before;
			node->prev = originalPrev;
			if (originalPrev != NULL)
			{
				originalPrev->next = node;
			}
			else
			{
				node->list->first = node;
			}
			count += 1;
			return node;
		}
		linkednode<T>* AddAfter(T item, linkednode<T>* after)
		{
			linkednode<T>* node = (linkednode<T>*)allocator->Allocate(sizeof(linkednode<T>));
			node->list = this;
			node->value = item;
			linkednode<T>* originalNext = after->next;
			after->next = node;
			node->prev = after;
			node->next = originalNext;
			if (originalNext != NULL)
			{
				originalNext->prev = node;
			}
			else
			{
				node->list->last = node;
			}
			count += 1;
			return node;
		}
		linkednode<T>* Append(T item)
		{
			if (this->last != NULL)
			{
				return AddAfter(item, this->last);
			}
			linkednode<T>* node = (linkednode<T>*)allocator->Allocate(sizeof(linkednode<T>));
			node->list = this;
			node->value = item;
			node->prev = NULL;
			node->next = NULL;
			this->first = node;
			this->last = node;
			count += 1;
			return node;
		}
		linkednode<T>* Prepend(T item)
		{
			if (this->first != NULL)
			{
				return AddBefore(item, this->first);
			}
			linkednode<T>* node = (linkednode<T>*)allocator->Allocate(sizeof(linkednode<T>));
			node->list = this;
			node->value = item;
			node->prev = NULL;
			node->next = NULL;
			this->first = node;
			this->last = node;
			count += 1;
			return node;
		}
		void Remove(linkednode<T>* node, bool dispose = true)
		{
			if (node->next == NULL)
			{
				node->list->last = node->prev;
			}
			else
			{
				node->next->prev = node->prev;
			}

			if (node->prev == NULL)
			{
				node->list->first = node->next;
			}
			else
			{
				node->prev->next = node->next;
			}
			node->list->count -= 1;
			if (dispose)
			{
				node->list->allocator.Free(node);
			}
		}
		void Clear_Free()
		{
			linkednode<T>* node = this->first;
			while (node != NULL)
			{
				linkednode<T>* next = node->next;
				this->allocator->Free(node);
				node = next;
			}
			count = 0;
		}
	};
}

#endif