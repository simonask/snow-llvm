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

	INLINE bool is_integer(Immediate val) {
		return ((intptr_t)val.value() & 1) != 0;
	}

	INLINE bool is_float(const Immediate& val) {
		return ((intptr_t)val.value() & ValueTypeMask) == FloatType;
	}

	INLINE Immediate integer_to_value(int n) {
		return (VALUE)(((intptr_t)n << 1) | 1);
	}
	INLINE int value_to_integer(const Immediate& val) {
		ASSERT(is_integer(val));
		return (int)((int64_t)val.value() >> 1);
	}

	Value big_integer_to_value(const void* bytes, size_t n_bytes);
	void extract_big_integer(void* destination_bytes, size_t n_bytes, const Value& big_integer);

	Immediate float_to_value(float f) {
		uintptr_t n = *(uintptr_t*)&f;
		return (VALUE)((n << 16) | FloatType);
	}

	INLINE float value_to_float(const Immediate& val) {
		uintptr_t n = (uintptr_t)val.value() >> 16;
		float* f = (float*)&n;
		return *f;
	}
}

#endif /* end of include guard: NUMERIC_H_TFV19DYH */
