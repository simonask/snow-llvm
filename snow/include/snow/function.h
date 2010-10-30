#pragma once
#ifndef FUNCTION_H_X576C5TP
#define FUNCTION_H_X576C5TP

#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/value.h"
#include "snow/object.h"


struct SnFunctionCallContext;
struct SnFunction;
struct SnArguments;

typedef VALUE(*SnFunctionPtr)(struct SnFunctionCallContext* here, VALUE self, VALUE it);

typedef struct SnFunctionSignature {
	SnType return_type;
	size_t num_params;
	SnType* param_types;
	SnSymbol* param_names;
} SnFunctionSignature;

typedef struct SnFunctionDescriptor {
	int64_t ref_count;
	void* jit_handle;
	SnFunctionSignature* signature;
	SnFunctionPtr ptr;
	SnSymbol* local_names;
	uint32_t num_locals;
} SnFunctionDescriptor;

typedef struct SnFunctionCallContext {
	SnObjectBase base;
	struct SnFunction* function;
	struct SnFunctionCallContext* caller;
	VALUE self;
	struct SnArguments* arguments;
	VALUE* locals;
} SnFunctionCallContext;

typedef struct SnFunction {
	SnObjectBase base;
	SnFunctionDescriptor* descriptor;
	SnFunctionCallContext* definition_context;
} SnFunction;

CAPI SnFunction* snow_create_function(SnFunctionDescriptor* descriptor);

CAPI VALUE snow_function_call(SnFunction* function, VALUE self, struct SnArguments* args);

#endif /* end of include guard: FUNCTION_H_X576C5TP */
