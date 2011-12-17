#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include "snow/type.h"
#include <stdlib.h>

struct SnObject;
struct SnInternalType;

typedef enum SnGCFlags {
	SnGCNoFlags   = 0x0,
	SnGCAllocated = 0x1,
	SnGCReachable = 0x2,
} SnGCFlags;

CAPI void snow_init_gc(const void** stack_top);
CAPI void snow_gc();
CAPI struct SnObject* snow_gc_allocate_object(const struct SnInternalType*);
CAPI struct SnObject** snow_gc_create_root(struct SnObject* initial_value);
CAPI struct SnObject*  snow_gc_free_root(struct SnObject** root); 

#define SN_GC_RDLOCK(OBJECT) snow_gc_rdlock((const struct SnObject*)(OBJECT), &(OBJECT))
#define SN_GC_WRLOCK(OBJECT) snow_gc_wrlock((const struct SnObject*)(OBJECT), &(OBJECT))
#define SN_GC_UNLOCK(OBJECT) snow_gc_unlock((const struct SnObject*)(OBJECT))

CAPI void snow_gc_rdlock(const struct SnObject* object, void* gc_root);
CAPI void snow_gc_wrlock(const struct SnObject* object, void* gc_root);
CAPI void snow_gc_unlock(const struct SnObject* object);

#endif /* end of include guard: GC_H_X9TH74GE */
