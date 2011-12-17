#pragma once
#ifndef ARRAY_H_XVGE8JQI
#define ARRAY_H_XVGE8JQI

#include "snow/object.h"

CAPI SnInternalType SnArrayType;

CAPI SnObject* snow_create_array();
CAPI SnObject* snow_create_array_from_range(VALUE* begin, VALUE* end);
CAPI SnObject* snow_create_array_with_size(uint32_t alloc_size);

CAPI size_t snow_array_size(const SnObject* array);
CAPI VALUE snow_array_get(const SnObject* array, int32_t i);
CAPI VALUE snow_array_set(SnObject* array, int32_t i, VALUE val);
CAPI size_t snow_array_get_all(const SnObject* array, VALUE* out_values, size_t max);
CAPI void snow_array_reserve(SnObject* array, uint32_t new_alloc_size);
CAPI SnObject* snow_array_push(SnObject* array, VALUE val);
CAPI SnObject* snow_array_push_range(SnObject* array, VALUE* begin, VALUE* end);
CAPI SnObject* snow_array_concatenate(const SnObject* a, const SnObject* b);
CAPI bool snow_array_contains(const SnObject* a, VALUE val);
CAPI int32_t snow_array_index_of(const SnObject* a, VALUE val);

CAPI struct SnObject* snow_get_array_class();

#endif /* end of include guard: ARRAY_H_XVGE8JQI */
