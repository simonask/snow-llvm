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

typedef struct SnFunctionDescriptor {
	SnFunctionPtr ptr;
	SnType return_type;
	size_t num_params;
	SnType* param_types;
	SnSymbol* param_names;
	int it_index; // index for the `it` argument -- necessary because param_names
	SnSymbol* local_names;
	uint32_t num_locals; // num_locals >= num_params (locals include arguments)
	bool needs_context;
} SnFunctionDescriptor;

typedef struct SnFunctionCallContext {
	// XXX: Order matters -- codegen depends on this specific ordering of members.
	SnObjectBase base;
	struct SnFunction* function;
	struct SnFunctionCallContext* caller;
	VALUE self;
	
	/*
		Organization of the `locals` array:
		-->    function arguments, in named order
		       ...
		       locals in the function
		       ...
		       extra named function arguments, in named order
		       ...
		       extra unnamed function arguments, in provided order
	*/
	VALUE* locals; // size: num_locals (including num_params) + num_extra_named_args + num_extra_unnamed_args

	size_t num_extra_names;
	size_t num_extra_args; // num_extra_args >= num_extra_names
	SnSymbol* extra_names; // names for named arguments that aren't specified in the function signature
} SnFunctionCallContext;

typedef struct SnFunction {
	SnObjectBase base;
	const SnFunctionDescriptor* descriptor;
	SnFunctionCallContext* definition_context;
} SnFunction;

CAPI SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnFunctionCallContext* definition_context);
CAPI SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args);
CAPI SnFunction* snow_value_to_function(VALUE val);

CAPI VALUE snow_function_call(SnFunction* function, SnFunctionCallContext* context, VALUE self, VALUE it);

#endif /* end of include guard: FUNCTION_H_X576C5TP */
