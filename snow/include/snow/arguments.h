#pragma once
#ifndef ARGUMENTS_H_5M3JAXPP
#define ARGUMENTS_H_5M3JAXPP

#include "snow/object.h"
#include "snow/symbol.h"

struct SnFunctionDescriptor;
struct SnArray;
struct SnMap;

typedef struct SnArguments {
	SnObjectBase base;
	const struct SnFunctionDescriptor* descriptor;
	SnSymbol* extra_names;
	size_t num_extra_names;
	VALUE* data;
	size_t size;
} SnArguments;

/*
	Layout of SnArguments.data:
	1. arguments named in the descriptor
	2. extra named arguments
	3. unnamed arguments
*/

CAPI SnArguments* snow_create_arguments(size_t num_names, size_t size);
CAPI SnArguments* snow_create_arguments_for_function_call(const struct SnFunctionDescriptor*, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args);
CAPI void snow_arguments_grow_by(SnArguments* args, size_t num_names, size_t size);
INLINE size_t snow_arguments_size(const SnArguments* args) { return args->size; }
INLINE VALUE snow_arguments_get_by_index(SnArguments* args, size_t idx) { return idx < args->size ? args->data[idx] : NULL; }
CAPI VALUE snow_arguments_get_by_name(SnArguments* args, SnSymbol sym);
CAPI void snow_arguments_merge(SnArguments* args, const SnArguments* other);
CAPI void snow_arguments_append_array(SnArguments* args, const struct SnArray* other);
CAPI void snow_arguments_append_map(SnArguments* args, const struct SnMap* other);

#endif /* end of include guard: ARGUMENTS_H_5M3JAXPP */
