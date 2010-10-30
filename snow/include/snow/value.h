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
	SnFunctionCallContextType,
	SnPointerType,
	
	SnNumTypes
} SnType;

static const VALUE SN_NIL = (VALUE)SnNilType;
static const VALUE SN_FALSE = (VALUE)SnFalseType;
static const VALUE SN_TRUE = (VALUE)SnTrueType;



CAPI SnType snow_type_of(VALUE val);
CAPI bool snow_is_object(VALUE val);

#endif /* end of include guard: VALUE_H_ATXVBTXI */
