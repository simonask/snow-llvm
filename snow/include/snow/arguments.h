#pragma once
#ifndef ARGUMENTS_H_TR5H2OYE
#define ARGUMENTS_H_TR5H2OYE

#include "snow/symbol.h"

#include <alloca.h>

struct SnFunction;
struct SnFunctionSignature;

typedef struct SnArguments {
	VALUE* values;
	VALUE* names; // symbols or NULLs for unnamed arguments
	size_t size;
	struct SnFunctionSignature* signature; // for type info
} SnArguments;


CAPI void snow_arguments_set_named(SnArguments* arguments, SnSymbol name, VALUE value);
CAPI void snow_arguments_push(SnArguments* arguments, VALUE value);
CAPI VALUE snow_arguments_get_named(const SnArguments* arguments, SnSymbol name);
CAPI VALUE snow_arguments_get(const SnArguments* arguments, uint32_t idx);
CAPI size_t snow_arguments_size(const SnArguments* arguments);


#define SN_ALLOC_ARGUMENTS(NAME, N, FUNCTION) \
	(NAME) = (SnArguments*)alloca(sizeof(SnArguments)); \
	_snow_arguments_get_size_for_function(NAME, N, FUNCTION); \
	(NAME)->values = (VALUE*)alloca(((NAME)->size)*sizeof(VALUE)); \
	(NAME)->names = (VALUE*)alloca(((NAME)->size)*sizeof(VALUE)); \
	_snow_arguments_init(NAME) // copy names from function definition

CAPI void _snow_arguments_get_size_for_function(SnArguments* arguments, uint32_t requested_size, struct SnFunction* function);
CAPI void _snow_arguments_init(SnArguments* arguments);

#endif /* end of include guard: ARGUMENTS_H_TR5H2OYE */
