#pragma once
#ifndef FUNCTION_H_X576C5TP
#define FUNCTION_H_X576C5TP

#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/value.h"
#include "snow/object.h"
#include "snow/arguments.h"

struct SnFunctionCallContext;
struct SnFunction;

typedef VALUE(*SnFunctionPtr)(struct SnFunctionCallContext* here, VALUE self, VALUE it);

typedef struct SnFunctionDescriptor {
	SnFunctionPtr ptr;
	SnType return_type;
	size_t num_params;
	SnType* param_types;
	SnSymbol* param_names;
	int it_index; // index for the `it` argument -- necessary because param_names is sorted at compile-time
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
	VALUE* locals;  // locals in the function, including named args. size: descriptor.num_locals
	struct SnArguments* arguments;
} SnFunctionCallContext;

typedef struct SnFunction {
	SnObjectBase base;
	const SnFunctionDescriptor* descriptor;
	SnFunctionCallContext* definition_context;
} SnFunction;

CAPI SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnFunctionCallContext* definition_context);
CAPI SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args);
CAPI void snow_merge_splat_arguments(SnFunctionCallContext* callee_context, VALUE mergee);
CAPI SnFunction* snow_value_to_function(VALUE val);

CAPI VALUE snow_function_call(SnFunction* function, SnFunctionCallContext* context, VALUE self, VALUE it);

// Convenience for C bindings
CAPI SnFunction* snow_create_method(SnFunctionPtr function, int num_args);

#define SN_DEFINE_METHOD(PROTO, NAME, PTR, NUM_ARGS) snow_object_set_member(PROTO, PROTO, snow_sym(NAME), snow_create_method(PTR, NUM_ARGS))

#endif /* end of include guard: FUNCTION_H_X576C5TP */
