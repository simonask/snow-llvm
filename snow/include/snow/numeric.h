#pragma once
#ifndef NUMERIC_H_TFV19DYH
#define NUMERIC_H_TFV19DYH

#include "snow/value.h"

static inline bool snow_is_integer(VALUE val) {
	return ((intptr_t)val & 1) != 0;
}

static inline bool snow_is_float(VALUE val) {
	return ((intptr_t)val & SnTypeMask) == SnFloatType;
}

static inline VALUE snow_integer_to_value(int n) {
	return (VALUE)(((intptr_t)n << 1) | 1);
}
static inline int snow_value_to_integer(VALUE val) {
	ASSERT(snow_is_integer(val));
	return (int)((intptr_t)val >> 1);
}

CAPI VALUE snow_big_integer_to_value(const void* bytes, size_t n_bytes);
CAPI void snow_extract_big_integer(void* destination_bytes, size_t n_bytes, VALUE big_integer);

static inline VALUE snow_float_to_value(float f) {
	uintptr_t n = *(uintptr_t*)&f;
	return (VALUE)((n << 16) | SnFloatType);
}

static inline float snow_value_to_float(VALUE val) {
	uintptr_t n = (uintptr_t)val >> 16;
	float* f = (float*)&n;
	return *f;
}

#endif /* end of include guard: NUMERIC_H_TFV19DYH */
