#include "allocator.hpp"
#include "snow/gc.h"
#include <sys/mman.h>
#include <errno.h>
#include <new>

#define DEBUG_MEMORY_CORRUPTION 0

namespace snow {
	SnObjectBase* Allocator::allocate() {
		Block* p;
		if (!(p = find_available_block())) {
			p = create_block();
		}
		SnObjectBase* object = allocate_from_block(p);
		object->gc_flags = SnGCAllocated;
		object->gc_page_offset = p->block_offset_for((GCObject*)object);
		ASSERT(get_block_for_object_fast(object) == p);
		return object;
	}
	
	void Allocator::free(SnObjectBase* object) {
		free(object, get_block_for_object_fast(object));
	}
	
	void Allocator::free(SnObjectBase* object, Allocator::Block* block) {
		ASSERT(block->contains(object));
		ASSERT(object->gc_flags & SnGCAllocated);
		object->gc_flags &= ~SnGCAllocated;
		if (UNLIKELY(object == block->end-1)) {
			--block->end;
		} else {
			// append to free list
			*(GCObject***)object = block->free_list_head;
			block->free_list_head = (GCObject**)object;
			++block->free_list_size;
		}
	}
	
	bool Allocator::is_allocated(SnObjectBase* ptr) const {
		for (size_t i = 0; i < _blocks.size(); ++i) {
			if (_blocks[i]->contains(ptr))
				return (ptr->gc_flags & SnGCAllocated) != 0;
		}
		return false;
	}
	
	Allocator::Block* Allocator::get_block_for_object_fast(const SnObjectBase* ptr) const {
		intptr_t p = (intptr_t)ptr;
		intptr_t offset = ptr->gc_page_offset;
		intptr_t page_base = p - (p % SN_MEMORY_PAGE_SIZE);
		Block* block = (Block*)(page_base - offset*SN_MEMORY_PAGE_SIZE);
		ASSERT(block->contains(ptr));
		return block;
	}
	
	Allocator::Block* Allocator::get_block_for_object_safe(const SnObjectBase* ptr) const {
		intptr_t p = (intptr_t)ptr;
		intptr_t offset = ptr->gc_page_offset;
		intptr_t page_base = p % SN_MEMORY_PAGE_SIZE;
		Block* block = (Block*)(page_base - offset*SN_MEMORY_PAGE_SIZE);
		for (size_t i = 0; i < _blocks.size(); ++i) {
			if (_blocks[i] == block) {
				return block->contains(ptr) ? block : NULL;
			}
		}
		return NULL;
	}
	
	Allocator::Block* Allocator::find_available_block() {
		for (ssize_t i = _blocks.size()-1; i >= 0; --i) {
			if (_blocks[i]->num_available()) {
				return _blocks[i];
			}
		}
		return NULL;
	}
	
	Allocator::Block* Allocator::create_block() {
		byte* memory = (byte*)mmap(NULL, SN_ALLOCATION_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		if (UNLIKELY(memory == MAP_FAILED)) {
			perror("Memory allocation failed");
			exit(1);
		}
		
		Block* b = new(memory) Block;
		b->memory = memory + sizeof(Block);
		b->memory += SN_OBJECT_SIZE - (sizeof(Block) % SN_OBJECT_SIZE);
		b->end = b->begin;
		b->free_list_head = NULL;
		b->free_list_size = 0;
		
		_blocks.push_back(b);
		
		return b;
	}
	
	SnObjectBase* Allocator::allocate_from_block(Allocator::Block* p) {
		SnObjectBase* object;
		if (UNLIKELY(p->free_list_head != NULL)) {
			// pop from free list
			object = *p->free_list_head;
			p->free_list_head = *(GCObject***)object;
			--p->free_list_size;
		} else {
			ASSERT(p->num_available());
			ASSERT(p->end < p->begin + OBJECTS_PER_BLOCK);
			object = p->end++;
		}
		return object;
	}
	
	inline unsigned int Allocator::Block::block_offset_for(const Allocator::GCObject* ptr) const {
		ASSERT(contains(ptr));
		intptr_t p = (intptr_t)ptr;
		intptr_t page_base = p - (p % SN_MEMORY_PAGE_SIZE);
		intptr_t block_base = (intptr_t)this;
		unsigned int result = (page_base - block_base) / SN_MEMORY_PAGE_SIZE;
		return result;
	}
}