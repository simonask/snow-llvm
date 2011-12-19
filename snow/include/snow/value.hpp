#pragma once
#ifndef VALUE_H_ATXVBTXI
#define VALUE_H_ATXVBTXI

#include "snow/basic.h"

namespace snow {
	typedef void* VALUE;
	
	enum ValueType {
		ObjectType = 0x0,
		IntegerType = 0x1,
		NilType = 0x2,   // == SN_NIL
		BooleanType = 0x4, // == SN_FALSE
		// 0x6 unused
		SymbolType = 0x8,
		FloatType = 0xa,

		ValueTypeMask = 0xf,
		AnyType
	};
	
	static const VALUE SN_UNDEFINED = NULL;
	static const VALUE SN_NIL = (VALUE)NilType;
	static const VALUE SN_FALSE = (VALUE)BooleanType;
	static const VALUE SN_TRUE = (VALUE)(0x10 | BooleanType);
	
	INLINE bool is_immediate_type(ValueType type) {
		return type < ValueTypeMask && type != ObjectType;
	}

	INLINE bool is_object(VALUE val) {
		return val && (((uintptr_t)val & ValueTypeMask) == ObjectType);
	}

	INLINE bool is_immediate(VALUE val) {
		return !is_object(val);
	}

	INLINE bool is_truthy(VALUE val) {
		return val != NULL && val != SN_NIL && val != SN_FALSE;
	}
}

#endif /* end of include guard: VALUE_H_ATXVBTXI */
