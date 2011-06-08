#pragma once
#ifndef FUNCTION_H_X576C5TP
#define FUNCTION_H_X576C5TP

#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/value.h"
#include "snow/object.h"
#include "snow/arguments.h"

struct SnCallFrame;

typedef VALUE(*SnFunctionPtr)(const struct SnCallFrame* here, VALUE self, VALUE it);

CAPI SnObjectType SnFunctionType;
CAPI SnObjectType SnCallFrameType;

typedef struct SnCallFrame {
	SnObject* function;
	struct SnCallFrame* caller;
	VALUE self;
	VALUE* locals; // size: function->descriptor.num_locals
	SnArguments args;
	SnObject* as_object;
} SnCallFrame;

CAPI struct SnObject* snow_get_function_class();
CAPI struct SnObject* snow_get_call_frame_class();

CAPI SnObject* snow_create_function(SnFunctionPtr ptr, SnSymbol name); // TODO: Type inference info
CAPI SnObject* snow_call_frame_as_object(const SnCallFrame* frame);
CAPI VALUE* snow_get_locals_from_higher_lexical_scope(const SnCallFrame* frame, size_t num_levels);
CAPI SnObject* snow_call_frame_get_function(const SnObject* obj);
CAPI VALUE* snow_call_frame_get_locals(const SnObject* obj);

CAPI void snow_merge_splat_arguments(SnCallFrame* callee_context, VALUE mergee);
CAPI SnObject* snow_value_to_function(VALUE val, VALUE* out_new_self);

CAPI VALUE snow_function_call(SnObject* function, SnCallFrame* frame);
CAPI SnSymbol snow_function_get_name(const SnObject* obj);
CAPI size_t snow_function_get_num_locals(const SnObject* obj);
CAPI SnObject* snow_function_get_definition_scope(const SnObject* obj);

// Convenience for C bindings
CAPI SnObject* snow_create_method_proxy(VALUE self, VALUE method);

#endif /* end of include guard: FUNCTION_H_X576C5TP */
