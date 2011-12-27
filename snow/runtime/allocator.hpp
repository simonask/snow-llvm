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
		friend class const_iterator;
		struct Placeholder {
			union {
				Placeholder* next;
				byte _[sizeof(T)];
			};
		};
		Placeholder* head_;
	public:
		class const_iterator {
		public:
			const_iterator() : p_(NULL) {}
			const_iterator(const const_iterator& other) : p_(other.p_) {}
			const_iterator& operator++() { p_ = p_->next; return *this; }
			const_iterator operator++(int) { const_iterator it = *this; p_ = p_->next; return it; }
			bool operator==(const const_iterator& other) const { return p_ == other.p_; }
			bool operator!=(const const_iterator& other) const { return p_ != other.p_; }
			void* operator*() const { return p_; }
		private:
			const_iterator(Placeholder* p) : p_(p) {}
			Placeholder* p_;
			friend class InPlaceFreeList<T>;
		};
		
		const_iterator begin() const { return const_iterator(head_); }
		const_iterator end() const { return const_iterator(); }
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
		size_t num_blocks() const { return blocks_.size(); }
		size_t capacity() const { return num_blocks() * OBJECTS_PER_BLOCK; }
		Object* find_object_and_index(VALUE val, size_t& out_index);
		size_t index_of_object(void* ptr);
		Object* object_at_index(size_t idx);
		
		InPlaceFreeList<Object>::const_iterator free_list_begin() const { return free_list_.begin(); }
		InPlaceFreeList<Object>::const_iterator free_list_end() const { return free_list_.end(); }
	private:
		InPlaceFreeList<Object> free_list_;
		std::deque<Block*> blocks_;
		
		Block* find_available_block();
		Block* create_block();
		GCObject* allocate_from_block(Block* block);
	};
	
	inline void Allocator::free(Object* object) {
		free_list_.push(object);
	}
	
	inline Object* Allocator::find_object_and_index(VALUE val, size_t& out_index) {
		byte* p = (byte*)val;
		for (size_t i = 0; i < blocks_.size(); ++i) {
			Block* block = blocks_[i];
			if (p >= block->begin && p < block->current) {
				ptrdiff_t diff = p - block->begin;
				size_t misalignment = diff % SN_OBJECT_SIZE;
				Object* obj = (Object*)(p - misalignment);
				ptrdiff_t offset = diff - misalignment;
				out_index = i * OBJECTS_PER_BLOCK + (offset / SN_OBJECT_SIZE);
				return obj;
			}
		}
		return NULL;
	}
	
	inline size_t Allocator::index_of_object(void* ptr) {
		size_t idx;
		Object* obj = find_object_and_index(ptr, idx);
		ASSERT(obj != NULL);
		return idx;
	}
	
	inline Object* Allocator::object_at_index(size_t idx) {
		size_t block_index = idx / OBJECTS_PER_BLOCK;
		size_t offset = idx % OBJECTS_PER_BLOCK;
		if (block_index < blocks_.size()) {
			Block* block = blocks_[block_index];
			byte* p = block->begin + (offset * SN_OBJECT_SIZE);
			if (p < block->current) {
				return (Object*)p;
			}
		}
		return NULL;
	}
}

#endif /* end of include guard: ALLOCATOR_HPP_UZ4GOM1J */
