#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include "snow/type.h"
#include <stdlib.h>

struct SnProcess;
struct SnObjectBase;

typedef enum SnGCFlags {
	SnGCAllocated = 0x1,
	SnGCReachable = 0x2,
} SnGCFlags;

CAPI void snow_init_gc(const void** stack_top);
CAPI void snow_gc();
CAPI struct SnObjectBase* snow_gc_alloc_object(size_t sz, SnType type);
CAPI struct SnObjectBase* snow_gc_alloc_fixed(size_t sz, SnType type);
CAPI void snow_gc_free_fixed(void* object);

#define SN_GC_ALLOC_OBJECT(TYPE) (struct TYPE*)snow_gc_alloc_object(sizeof(struct TYPE), TYPE ## Type)
#define SN_GC_ALLOC_FIXED(TYPE) (struct TYPE*)snow_gc_alloc_fixed(sizeof(struct TYPE), TYPE ## Type)
#define SN_GC_FREE_FIXED(OBJECT) snow_gc_free_fixed(OBJECT)
#define SN_GC_RDLOCK(OBJECT) snow_gc_rdlock(&(OBJECT)->base, &(OBJECT))
#define SN_GC_WRLOCK(OBJECT) snow_gc_wrlock(&(OBJECT)->base, &(OBJECT))
#define SN_GC_UNLOCK(OBJECT) snow_gc_unlock(&(OBJECT)->base)

CAPI void snow_gc_rdlock(const SnObjectBase* object, void* gc_root);
CAPI void snow_gc_wrlock(const SnObjectBase* object, void* gc_root);
CAPI void snow_gc_unlock(const SnObjectBase* object);

#endif /* end of include guard: GC_H_X9TH74GE */
