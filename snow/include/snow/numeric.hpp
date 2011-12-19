#pragma once
#ifndef NUMERIC_H_TFV19DYH
#define NUMERIC_H_TFV19DYH

#include "snow/value.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Class;
	
	ObjectPtr<Class> get_numeric_class();
	ObjectPtr<Class> get_integer_class();
	ObjectPtr<Class> get_float_class();

	INLINE bool is_integer(VALUE val) {
		return ((intptr_t)val & 1) != 0;
	}

	INLINE bool is_float(VALUE val) {
		return ((intptr_t)val & ValueTypeMask) == FloatType;
	}

	INLINE VALUE integer_to_value(int n) {
		return (VALUE)(((intptr_t)n << 1) | 1);
	}
	INLINE int value_to_integer(VALUE val) {
		ASSERT(is_integer(val));
		return (int)((int64_t)val >> 1);
	}

	CAPI VALUE big_integer_to_value(const void* bytes, size_t n_bytes);
	CAPI void extract_big_integer(void* destination_bytes, size_t n_bytes, VALUE big_integer);

	INLINE VALUE float_to_value(float f) {
		uintptr_t n = *(uintptr_t*)&f;
		return (VALUE)((n << 16) | FloatType);
	}

	INLINE float value_to_float(VALUE val) {
		uintptr_t n = (uintptr_t)val >> 16;
		float* f = (float*)&n;
		return *f;
	}
}

#endif /* end of include guard: NUMERIC_H_TFV19DYH */
