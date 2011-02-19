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

#define SN_GC_ALLOC_OBJECT(TYPE) (struct TYPE*)snow_gc_alloc_object(sizeof(struct TYPE), TYPE ## Type)

#endif /* end of include guard: GC_H_X9TH74GE */
