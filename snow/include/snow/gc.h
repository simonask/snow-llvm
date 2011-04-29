#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include "snow/type.h"
#include <stdlib.h>

struct SnProcess;
struct SnObject;

typedef enum SnGCFlags {
	SnGCNoFlags   = 0x0,
	SnGCAllocated = 0x1,
	SnGCReachable = 0x2,
} SnGCFlags;

typedef struct SnGCInfo {
	union {
		struct {
			unsigned flags       : 8;
			unsigned page_offset : 8;
		};
		
		void* _; // padding
	};
} SnGCInfo;

CAPI void snow_init_gc(const void** stack_top);
CAPI void snow_gc();
CAPI struct SnObject* snow_gc_alloc_object(SnType internal_type);
CAPI VALUE* snow_gc_create_root(VALUE initial_value);
CAPI VALUE  snow_gc_free_root(VALUE* root); 

#define SN_GC_ALLOC_OBJECT(TYPE) (struct TYPE*)snow_gc_alloc_object(TYPE ## Type)
#define SN_GC_RDLOCK(OBJECT) snow_gc_rdlock((const struct SnObject*)(OBJECT), &(OBJECT))
#define SN_GC_WRLOCK(OBJECT) snow_gc_wrlock((const struct SnObject*)(OBJECT), &(OBJECT))
#define SN_GC_UNLOCK(OBJECT) snow_gc_unlock((const struct SnObject*)(OBJECT))

CAPI void snow_gc_rdlock(const struct SnObject* object, void* gc_root);
CAPI void snow_gc_wrlock(const struct SnObject* object, void* gc_root);
CAPI void snow_gc_unlock(const struct SnObject* object);

#endif /* end of include guard: GC_H_X9TH74GE */
