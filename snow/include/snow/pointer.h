#pragma once
#ifndef POINTER_H_EZG3FT77
#define POINTER_H_EZG3FT77

#include "snow/basic.h"
#include "snow/object.h"

typedef void*(*SnPointerCopyFunc)(const void* ptr);
typedef void(*SnPointerFreeFunc)(void* ptr);

typedef struct SnPointer {
	SnObjectBase base;
	void* ptr;
	SnPointerCopyFunc copy;
	SnPointerFreeFunc free;
} SnPointer;

CAPI SnPointer* snow_wrap_pointer(void* ptr, SnPointerCopyFunc copy_func, SnPointerFreeFunc free_func);
CAPI SnPointer* snow_pointer_copy(SnPointer* ptr);
static inline void* snow_pointer_get(SnPointer* ptr) { return ptr->ptr; }
CAPI void snow_pointer_free(SnPointer* ptr);

#endif /* end of include guard: POINTER_H_EZG3FT77 */
