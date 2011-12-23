#include "allocator.hpp"
#include "snow/gc.hpp"
#include <sys/mman.h>
#include <errno.h>
#include <new>

#define DEBUG_MEMORY_CORRUPTION 0

namespace snow {
	Object* Allocator::allocate() {
		if (free_list_.is_empty()) {
			Block* p = find_available_block();
			if (p == NULL) {
				p = create_block();
			}
			return allocate_from_block(p);
		} else {
			return free_list_.pop();
		}
	}
	
	Allocator::Block* Allocator::find_available_block() {
		for (size_t i = 0; i < blocks_.size(); ++i) {
			if (blocks_[i]->num_available() >= SN_OBJECT_SIZE) {
				return blocks_[i];
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
		b->begin = memory + sizeof(Block);
		b->begin += SN_OBJECT_SIZE - (sizeof(Block) % SN_OBJECT_SIZE); // padding
		b->current = b->begin;
		b->end = memory + SN_ALLOCATION_BLOCK_SIZE;
		
		blocks_.push_front(b);
		
		return b;
	}
	
	Allocator::GCObject* Allocator::allocate_from_block(Allocator::Block* p) {
		ASSERT(p->num_available());
		ASSERT(p->current <= p->end);
		GCObject* object = (GCObject*)p->current;
		p->current += SN_OBJECT_SIZE;
		new(object) GCObject;
		return object;
	}
}