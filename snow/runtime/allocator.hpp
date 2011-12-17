#pragma once
#ifndef ALLOCATOR_HPP_UZ4GOM1J
#define ALLOCATOR_HPP_UZ4GOM1J

#include "snow/basic.h"
#include "snow/object.hpp"
#include "lock.h"

#include <vector>

namespace snow {
	class Allocator {
	public:
		struct GCObject : SnObject {
			union {
				byte _[SN_CACHE_LINE_SIZE - sizeof(SnObject)];
				struct {
					GCObject* next_free;
				};
			};
		};
		
		struct Block {
			SN_RWLOCK_T lock;
			union {
				byte* memory;
				GCObject* begin;
			};
			GCObject* end;
			GCObject* free_list_head;
			size_t free_list_size;
			
			Block() { SN_RWLOCK_INIT(&lock); }
			~Block() { SN_RWLOCK_DESTROY(&lock); }
			bool contains(const SnObject* ptr) const { return ptr >= begin && ptr < end; }
			size_t num_allocated_upper_bound() const { return end - begin; }
			size_t num_allocated() const { return num_allocated_upper_bound() - free_list_size; }
			size_t num_available() const;
			unsigned int block_offset_for(const GCObject* ptr) const;
			bool is_allocated(const SnObject* ptr) const;
		};
		
		static const size_t OBJECTS_PER_BLOCK = (SN_ALLOCATION_BLOCK_SIZE - sizeof(Block)) / SN_OBJECT_SIZE;
		
		SnObject* allocate();
		void free(SnObject* object);
		void free(SnObject* object, Block* block);
		bool is_allocated(SnObject* object) const;
		size_t get_num_blocks() const { return _blocks.size(); }
		size_t get_max_num_objects() const { return get_num_blocks() * OBJECTS_PER_BLOCK; }
		Block* get_block(unsigned i) { return _blocks[i]; }
		Block* get_block_for_object_fast(const SnObject* definitely_an_object) const;
		Block* get_block_for_object_safe(const SnObject* maybe_an_object, size_t* block_index = NULL, size_t* object_index = NULL) const;
		bool contains(void* ptr) const { return get_block_for_object_safe((const SnObject*)ptr) != NULL; }
	private:
		std::vector<Block*> _blocks;
		
		Block* find_available_block();
		Block* create_block();
		SnObject* allocate_from_block(Block* block);
		Block* get_block_for_object(const SnObject* probably_an_object) const;
	};
	
	inline size_t Allocator::Block::num_available() const {
		return Allocator::OBJECTS_PER_BLOCK - num_allocated();
	}
}

#endif /* end of include guard: ALLOCATOR_HPP_UZ4GOM1J */
