#pragma once
#ifndef GC_H_X9TH74GE
#define GC_H_X9TH74GE

#include "snow/basic.h"
#include <stdlib.h>

struct SnProcess;
struct SnObject;
struct SnType;

CAPI void snow_gc(SN_P);
CAPI struct SnObject* snow_gc_create_object(SN_P, const struct SnType*);

#endif /* end of include guard: GC_H_X9TH74GE */
