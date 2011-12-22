#pragma once
#ifndef ALLOCATOR_HPP_UZ4GOM1J
#define ALLOCATOR_HPP_UZ4GOM1J

#include "snow/basic.h"
#include "snow/object.hpp"

#include <deque>
#include <new>

namespace snow {
	template <typename T>
	class InPlaceFreeList {
	public:
		InPlaceFreeList() : head_(NULL) {}
		
		void push(T* t) {
			t->~T();
			Placeholder* p = reinterpret_cast<Placeholder*>(t);
			p->next = head_;
			head_ = p;
		}
		T* pop() {
			if (head_ != NULL) {
				T* t = reinterpret_cast<T*>(head_);
				head_ = head_->next;
				new(t) T;
				return t;
			}
			return NULL;
		}
		bool is_empty() const { return head_ == NULL; }
	private:
		struct Placeholder {
			union {
				Placeholder* next;
				byte _[sizeof(T)];
			};
		};
		Placeholder* head_;
	};
	
	
	class Allocator {
		struct GCObject : Object {
			byte padding[SN_CACHE_LINE_SIZE - sizeof(Object)];
		};
		
		struct Block {
			byte* begin;
			byte* end;
			byte* current;
			size_t num_available() const { return end - current; }
		};
	public:
		
		static const size_t OBJECTS_PER_BLOCK = (SN_ALLOCATION_BLOCK_SIZE - sizeof(Block)) / SN_OBJECT_SIZE;
		
		Object* allocate();
		void free(Object* object);
	private:
		InPlaceFreeList<GCObject> free_list_;
		std::deque<Block*> blocks_;
		
		Block* find_available_block();
		Block* create_block();
		GCObject* allocate_from_block(Block* block);
	};
}

#endif /* end of include guard: ALLOCATOR_HPP_UZ4GOM1J */
