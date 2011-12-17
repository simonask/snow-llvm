#pragma once
#ifndef NUMERIC_H_TFV19DYH
#define NUMERIC_H_TFV19DYH

#include "snow/value.hpp"

CAPI struct SnObject* snow_get_numeric_class();
CAPI struct SnObject* snow_get_integer_class();
CAPI struct SnObject* snow_get_float_class();

INLINE bool snow_is_integer(VALUE val) {
	return ((intptr_t)val & 1) != 0;
}

INLINE bool snow_is_float(VALUE val) {
	return ((intptr_t)val & SnValueTypeMask) == SnFloatType;
}

INLINE VALUE snow_integer_to_value(int n) {
	return (VALUE)(((intptr_t)n << 1) | 1);
}
INLINE int snow_value_to_integer(VALUE val) {
	ASSERT(snow_is_integer(val));
	return (int)((int64_t)val >> 1);
}

CAPI VALUE snow_big_integer_to_value(const void* bytes, size_t n_bytes);
CAPI void snow_extract_big_integer(void* destination_bytes, size_t n_bytes, VALUE big_integer);

INLINE VALUE snow_float_to_value(float f) {
	uintptr_t n = *(uintptr_t*)&f;
	return (VALUE)((n << 16) | SnFloatType);
}

INLINE float snow_value_to_float(VALUE val) {
	uintptr_t n = (uintptr_t)val >> 16;
	float* f = (float*)&n;
	return *f;
}

#endif /* end of include guard: NUMERIC_H_TFV19DYH */
