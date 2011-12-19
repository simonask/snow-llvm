#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include <stdlib.h>

struct SnObject;

namespace snow {
	struct Type;

	typedef enum GCFlags {
		GCNoFlags   = 0x0,
		GCAllocated = 0x1,
		GCReachable = 0x2,
	} GCFlags;

	void init_gc(const void** stack_top);
	void gc();
	struct SnObject* gc_allocate_object(const snow::Type*);
	struct SnObject** gc_create_root(struct SnObject* initial_value);
	struct SnObject*  gc_free_root(struct SnObject** root); 

	#define SN_GC_RDLOCK(OBJECT) gc_rdlock((const struct SnObject*)(OBJECT), &(OBJECT))
	#define SN_GC_WRLOCK(OBJECT) gc_wrlock((const struct SnObject*)(OBJECT), &(OBJECT))
	#define SN_GC_UNLOCK(OBJECT) gc_unlock((const struct SnObject*)(OBJECT))

	void gc_rdlock(const struct SnObject* object, void* gc_root);
	void gc_wrlock(const struct SnObject* object, void* gc_root);
	void gc_unlock(const struct SnObject* object);
}

#endif /* end of include guard: GC_H_X9TH74GE */
