#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include "snow/type.h"
#include <stdlib.h>

struct SnProcess;
struct SnObjectBase;

CAPI void snow_gc();
CAPI struct SnObjectBase* snow_gc_alloc_object(SnType type);

#endif /* end of include guard: GC_H_X9TH74GE */
