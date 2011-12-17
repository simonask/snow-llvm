#pragma once
#ifndef VALUE_H_ATXVBTXI
#define VALUE_H_ATXVBTXI

#include "snow/basic.h"

typedef void* VALUE;

typedef enum SnValueType {
	SnObjectType = 0x0,
	SnIntegerType = 0x1,
	SnNilType = 0x2,   // == SN_NIL
	SnBooleanType = 0x4, // == SN_FALSE
	// 0x6 unused
	SnSymbolType = 0x8,
	SnFloatType = 0xa,
	
	SnValueTypeMask = 0xf,
	SnAnyType
} SnValueType;

static const VALUE SN_UNDEFINED = NULL;
static const VALUE SN_NIL = (VALUE)SnNilType;
static const VALUE SN_FALSE = (VALUE)SnBooleanType;
static const VALUE SN_TRUE = (VALUE)(0x10 | SnBooleanType);

INLINE bool snow_is_object(VALUE val) {
	return val && (((uintptr_t)val & SnValueTypeMask) == SnObjectType);
}

INLINE bool snow_is_immediate(VALUE val) {
	return !snow_is_object(val);
}

INLINE bool snow_eval_truth(VALUE val) {
	return val != NULL && val != SN_NIL && val != SN_FALSE;
}

#endif /* end of include guard: VALUE_H_ATXVBTXI */
