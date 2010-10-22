#pragma once
#ifndef FUNCTION_H_X576C5TP
#define FUNCTION_H_X576C5TP

#include "snow/array.h"
#include "snow/symbol.h"

struct SnObject;
struct SnType;
struct SnFunctionSignature;

CAPI const struct SnType* snow_get_function_type();

typedef struct SnFunctionRef {
	struct SnObject* obj;
} SnFunctionRef;

CAPI SnFunctionRef snow_create_function(SN_P, void* jit_function);
CAPI struct SnFunctionSignature* snow_function_get_signature(SN_P, SnFunctionRef ref);

CAPI SnFunctionRef snow_object_as_function(struct SnObject* obj);
CAPI struct SnObject* snow_function_as_object(SnFunctionRef ref);

typedef struct SnFunctionParameter {
	SnSymbol name;
	struct SnType* expected_type;
} SnFunctionParameter;

typedef struct SnFunctionSignature {
	struct SnType* return_type;
	size_t num_params;
	SnFunctionParameter params[];
} SnFunctionSignature;

#endif /* end of include guard: FUNCTION_H_X576C5TP */
