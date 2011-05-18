#pragma once
#ifndef FUNCTION_H_X576C5TP
#define FUNCTION_H_X576C5TP

#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/value.h"
#include "snow/object.h"
#include "snow/arguments.h"

struct SnCallFrame;
struct SnFunction;
struct SnClass;

typedef VALUE(*SnFunctionPtr)(struct SnFunction* function, struct SnCallFrame* here, VALUE self, VALUE it);

typedef struct SnVariableReference {
	int level; // The number of call scopes to go up to find the variable.
	int index; // The index of the variable in that scope.
} SnVariableReference;

typedef struct SnFunctionDescriptor {
	SnFunctionPtr ptr;
	SnSymbol name;
	SnType return_type;
	size_t num_params;
	SnType* param_types;
	SnSymbol* param_names;
	int it_index; // index for the `it` argument -- necessary because param_names is sorted at compile-time
	SnSymbol* local_names;
	uint32_t num_locals; // num_locals >= num_params (locals include arguments)
	bool needs_context;
	void* jit_info;
	size_t num_variable_references;
	SnVariableReference* variable_references;
} SnFunctionDescriptor;

typedef struct SnCallFrame {
	// XXX: Order matters -- codegen depends on this specific ordering of members.
	SnObject base;
	struct SnFunction* function;
	struct SnCallFrame* caller;
	VALUE self;
	VALUE* locals;  // locals in the function, including named args. size: descriptor.num_locals
	struct SnArguments* arguments;
	SnObject* module;
} SnCallFrame;

typedef struct SnFunction {
	SnObject base;
	const SnFunctionDescriptor* descriptor;
	SnCallFrame* definition_context;
	VALUE** variable_references; // size: descriptor->num_variable_references. TODO: Consider garbage collection?
} SnFunction;

CAPI SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnCallFrame* definition_context);
CAPI SnCallFrame* snow_create_call_frame(SnFunction* callee, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args);
CAPI SnCallFrame* snow_create_call_frame_with_arguments(SnFunction* callee, SnArguments* args);
CAPI void snow_merge_splat_arguments(SnCallFrame* callee_context, VALUE mergee);
CAPI SnFunction* snow_value_to_function(VALUE val, VALUE* in_out_new_self);
CAPI struct SnClass* snow_get_function_class();
CAPI struct SnClass* snow_get_call_frame_class();

CAPI VALUE snow_function_call(SnFunction* function, SnCallFrame* context, VALUE self, VALUE it);

// Convenience for C bindings
CAPI SnFunction* snow_create_method(SnFunctionPtr function, SnSymbol name, int num_args);
CAPI SnObject* snow_create_method_proxy(VALUE self, VALUE method);

#define SN_DEFINE_METHOD(PROTO, NAME, PTR, NUM_ARGS) snow_set_member(PROTO, snow_sym(NAME), snow_create_method(PTR, snow_sym(#PTR), NUM_ARGS))

// Used by the GC
CAPI void snow_finalize_function(SnFunction*);
CAPI void snow_finalize_call_frame(SnCallFrame*);

#endif /* end of include guard: FUNCTION_H_X576C5TP */
