#pragma once
#ifndef ARGUMENTS_H_TR5H2OYE
#define ARGUMENTS_H_TR5H2OYE

#include "snow/symbol.h"

struct SnFunctionSignature;

typedef struct SnArguments {
	size_t size;
	VALUE* values;
	struct SnFunctionSignature* signature;
	size_t extra_size;
	SnSymbol* extra_names;
	VALUE* extra_values;
} SnArguments;

CAPI void snow_arguments_init(SN_P, SnArguments* args, struct SnFunctionSignature* sig, VALUE* begin, VALUE* end);
CAPI size_t snow_arguments_size(SN_P, const SnArguments* args);
CAPI VALUE snow_argument_get(SN_P, const SnArguments* args, int idx);
CAPI VALUE snow_argument_get_named(SN_P, const SnArguments* args, SnSymbol name);
CAPI void snow_argument_push(SN_P, SnArguments* args, VALUE val);
CAPI void snow_argument_push_all(SN_P, SnArguments* args, VALUE* begin, VALUE* end);
CAPI void snow_argument_set_named(SN_P, SnArguments* args, SnSymbol name, VALUE val);
CAPI void snow_arguments_merge(SN_P p, SnArguments* args, const SnArguments* other);

#endif /* end of include guard: ARGUMENTS_H_TR5H2OYE */
