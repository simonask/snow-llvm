#include "allocator.hpp"
#include "snow/gc.h"
#include <sys/mman.h>
#include <errno.h>

#define DEBUG_MEMORY_CORRUPTION 0

namespace snow {
	SnObjectBase* Allocator::allocate() {
		Page* p;
		if (!(p = find_available_page())) {
			p = create_page();
		}
		SnObjectBase* object = allocate_from_page(p);
		object->gc_flags = SnGCAllocated;
		return object;
	}
	
	void Allocator::free(SnObjectBase* object) {
		free(object, get_page_for_object(object));
	}
	
	void Allocator::free(SnObjectBase* object, Allocator::Page* page) {
		ASSERT(page->contains(object));
		ASSERT(object->gc_flags & SnGCAllocated);
		object->gc_flags &= ~SnGCAllocated;
		if (UNLIKELY(object == page->end-1)) {
			--page->end;
		} else {
			// append to free list
			*(GCObject***)object = page->free_list_head;
			page->free_list_head = (GCObject**)object;
			++page->free_list_size;
		}
	}
	
	bool Allocator::is_allocated(SnObjectBase* ptr) const {
		for (size_t i = 0; i < _pages.size(); ++i) {
			if (_pages[i].contains(ptr))
				return (ptr->gc_flags & SnGCAllocated) != 0;
		}
		return false;
	}
	
	Allocator::Page* Allocator::get_page_for_object(SnObjectBase* ptr) {
		for (size_t i = 0; i < _pages.size(); ++i) {
			if (_pages[i].contains(ptr)) {
				ASSERT(ptr->gc_flags & SnGCAllocated); // double free?
				return &_pages[i];
			}
		}
		return NULL;
	}
	
	Allocator::Page* Allocator::find_available_page() {
		for (size_t i = 0; i < _pages.size(); ++i) {
			if (_pages[i].num_available()) {
				return &_pages[i];
			}
		}
		return NULL;
	}
	
	Allocator::Page* Allocator::create_page() {
		// insert new page at the beginning
		if (LIKELY(_pages.size())) {
			// copy first page to end, and reuse the first object
			_pages.push_back(_pages[0]);
		} else {
			_pages.push_back(Page());
		}
		Page* p = &_pages[0];
		
		
#if DEBUG_MEMORY_CORRUPTION
		p->memory = mmap(NULL, SN_MEMORY_PAGE_SIZE*2, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		//fprintf(stderr, "Allocating map: %p, protecting: %p\n", p->memory, (byte*)p->memory + SN_MEMORY_PAGE_SIZE);
		mprotect((byte*)p->memory + SN_MEMORY_PAGE_SIZE, SN_MEMORY_PAGE_SIZE, PROT_NONE);
		p->end = p->begin + OBJECTS_PER_PAGE - 1; // only one object per page
#else
		p->memory = mmap(NULL, SN_MEMORY_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		if (UNLIKELY(p->memory == MAP_FAILED)) {
			perror("Memory allocation failed");
			exit(1);
		}
		p->end = p->begin;
#endif
		p->free_list_head = NULL;
		p->free_list_size = 0;
		return p;
	}
	
	SnObjectBase* Allocator::allocate_from_page(Allocator::Page* p) {
		SnObjectBase* object;
		if (UNLIKELY(p->free_list_head != NULL)) {
			// pop from free list
			object = *p->free_list_head;
			p->free_list_head = *(GCObject***)object;
			--p->free_list_size;
		} else {
			ASSERT(p->num_available());
			ASSERT(p->end < p->begin + OBJECTS_PER_PAGE);
			object = p->end++;
		}
		return object;
	}
}