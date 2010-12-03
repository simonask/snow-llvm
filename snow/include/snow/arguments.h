#pragma once
#ifndef ARGUMENTS_H_5M3JAXPP
#define ARGUMENTS_H_5M3JAXPP

#include "snow/object.h"
#include "snow/symbol.h"

struct SnFunctionDescriptor;

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
CAPI void snow_arguments_grow_by(SnArguments* args, size_t num_names, size_t size);

#endif /* end of include guard: ARGUMENTS_H_5M3JAXPP */
