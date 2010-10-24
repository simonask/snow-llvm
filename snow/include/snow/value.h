#pragma once
#ifndef VALUE_H_ATXVBTXI
#define VALUE_H_ATXVBTXI

#include "snow/basic.h"

typedef void* VALUE;

static const VALUE SN_UNDEFINED = (VALUE)0x0;
static const VALUE SN_NIL = (VALUE)0x2;
static const VALUE SN_FALSE = (VALUE)0x4;
static const VALUE SN_TRUE = (VALUE)0x6;

typedef enum SnValueType {
	SnPointerType = 0x0,
	SnIntegerType = 0x1,
	SnNilType = 0x2,   // == SN_NIL
	SnFalseType = 0x4, // == SN_FALSE
	SnTrueType = 0x6,  // == SN_TRUE
	SnSymbolType = 0x8,
	SnFloatType = 0xa,
	
	SnTypeMask = 0xf
} SnValueType;

static inline SnValueType snow_type_of_internal(VALUE val) { return (SnValueType)((uintptr_t)val & SnTypeMask); }

static inline SnValueType snow_type_of(VALUE val) {
	if (!val) return SnNilType;
	uintptr_t type = snow_type_of_internal(val);
	if (type == 0x11) type = SnIntegerType;
	return (SnValueType)type;
}

static inline bool snow_is_object(VALUE val) { return val && (snow_type_of_internal(val) == SnPointerType); }

static inline size_t snow_get_immediate_type_index(SnValueType type) {
	ASSERT(type != SnPointerType);
	return type / 2;
}

#endif /* end of include guard: VALUE_H_ATXVBTXI */
