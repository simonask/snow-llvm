#pragma once
#ifndef VALUE_H_ATXVBTXI
#define VALUE_H_ATXVBTXI

#include "snow/basic.h"

typedef void* VALUE;

typedef enum SnType {
	SnAnyType = 0x0,
	SnIntegerType = 0x1,
	SnNilType = 0x2,   // == SN_NIL
	SnFalseType = 0x4, // == SN_FALSE
	SnTrueType = 0x6,  // == SN_TRUE
	SnSymbolType = 0x8,
	SnFloatType = 0xa,
	
	SnTypeMask = 0xf,
	
	SnObjectType,
	SnStringType,
	SnArrayType,
	SnMapType,
	SnFunctionType,
	SnCallFrameType,
	SnArgumentsType,
	SnPointerType,
	SnFiberType,
	
	SnNumTypes
} SnType;

static const VALUE SN_NIL = (VALUE)SnNilType;
static const VALUE SN_FALSE = (VALUE)SnFalseType;
static const VALUE SN_TRUE = (VALUE)SnTrueType;

INLINE bool snow_is_object(VALUE val) {
	return val && (((uintptr_t)val & SnTypeMask) == 0);
}

INLINE bool snow_eval_truth(VALUE val) {
	return val != NULL && val != SN_NIL && val != SN_FALSE;
}

#endif /* end of include guard: VALUE_H_ATXVBTXI */
