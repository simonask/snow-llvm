#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include "snow/value.hpp"
#include <stdlib.h>

namespace snow {
	
	struct Object;
	struct Type;

	void init_gc(void** stack_top);
	void gc();
	struct Object* gc_allocate_object(const snow::Type*);
	Value* gc_create_root(Value initial_value = Value());
	Value  gc_free_root(Value* root); 
	
	void* allocate_memory(size_t size);
	void* reallocate_memory(void* ptr, size_t new_size);
	void free_memory(void* ptr);
	
	struct GCAlloc {
		void* operator new(size_t sz) {
			return allocate_memory(sz);
		}
		void operator delete(void* ptr) {
			free_memory(ptr);
		}
		void* operator new[](size_t sz) {
			return allocate_memory(sz);
		}
		void operator delete[](void* ptr) {
			free_memory(ptr);
		}
	};
}

#endif /* end of include guard: GC_H_X9TH74GE */
