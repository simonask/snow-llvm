#pragma once
#ifndef ALLOCATOR_HPP_UZ4GOM1J
#define ALLOCATOR_HPP_UZ4GOM1J

#include "snow/basic.h"
#include "snow/object.h"
#include "lock.h"

#include <vector>

namespace snow {
	class Allocator {
	public:
		struct GCObject : SnObjectBase {
			byte _[SN_OBJECT_SIZE - sizeof(SnObjectBase)];
		};
		
		struct Block {
			SN_RWLOCK_T lock;
			union {
				byte* memory;
				GCObject* begin;
			};
			GCObject* end;
			GCObject** free_list_head;
			size_t free_list_size;
			
			Block() { SN_RWLOCK_INIT(&lock); }
			~Block() { SN_RWLOCK_DESTROY(&lock); }
			bool contains(const SnObjectBase* ptr) const { return ptr >= begin && ptr < end; }
			size_t num_allocated() const { return (end - begin) - free_list_size; }
			size_t num_available() const;
			unsigned int block_offset_for(const GCObject* ptr) const;
		};
		
		static const size_t OBJECTS_PER_BLOCK = (SN_ALLOCATION_BLOCK_SIZE - sizeof(Block)) / SN_OBJECT_SIZE;
		
		SnObjectBase* allocate();
		void free(SnObjectBase* object);
		void free(SnObjectBase* object, Block* block);
		bool is_allocated(SnObjectBase* object) const;
		size_t get_num_blocks() const { return _blocks.size(); }
		Block* get_block(unsigned i) { return _blocks[i]; }
		Block* get_block_for_object_fast(const SnObjectBase* definitely_an_object) const;
		Block* get_block_for_object_safe(const SnObjectBase* maybe_an_object) const;
		bool contains(void* ptr) const { return get_block_for_object_safe((const SnObjectBase*)ptr) != NULL; }
	private:
		std::vector<Block*> _blocks;
		
		Block* find_available_block();
		Block* create_block();
		SnObjectBase* allocate_from_block(Block* block);
	};
	
	inline size_t Allocator::Block::num_available() const {
		return Allocator::OBJECTS_PER_BLOCK - num_allocated();
	}
}

#endif /* end of include guard: ALLOCATOR_HPP_UZ4GOM1J */
