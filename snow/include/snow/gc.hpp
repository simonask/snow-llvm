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
	void gc_free_object(Object* obj);
	Value* gc_create_root(Value initial_value = Value());
	Value  gc_free_root(Value* root); 
}

#endif /* end of include guard: GC_H_X9TH74GE */
