#pragma once
#ifndef ARRAY_H_XVGE8JQI
#define ARRAY_H_XVGE8JQI

#include "snow/object.h"

typedef struct SnArray {
	SnObjectBase base;
	VALUE* data;
	uint32_t size;
	uint32_t alloc_size;
} SnArray;

CAPI SnArray* snow_create_array();
CAPI SnArray* snow_create_array_from_range(VALUE* begin, VALUE* end);
CAPI SnArray* snow_create_array_with_size(size_t alloc_size);
CAPI SnArray* snow_array_copy(SnArray* array);

CAPI size_t snow_array_size(const SnArray* array);
CAPI VALUE snow_array_get(const SnArray* array, int32_t i);
CAPI VALUE snow_array_set(SnArray* array, int32_t i, VALUE val);
CAPI void snow_array_reserve(SnArray* array, uint32_t new_alloc_size);
CAPI SnArray* snow_array_push(SnArray* array, VALUE val);
CAPI SnArray* snow_array_push_range(SnArray* array, VALUE* begin, VALUE* end);
CAPI SnArray* snow_array_concatenate(SnArray* a, SnArray* b);
CAPI bool snow_array_contains(SnArray* a, VALUE value);

CAPI void snow_finalize_array(SnArray* a);

#endif /* end of include guard: ARRAY_H_XVGE8JQI */
