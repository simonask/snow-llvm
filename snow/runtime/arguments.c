#include "snow/arguments.h"
#include "snow/function.h"

#include <string.h>
#include <stdlib.h>

void snow_arguments_init(SN_P p, SnArguments* args, SnFunctionSignature* sig, VALUE* begin, VALUE* end) {
	memset(args, 0, sizeof(SnArguments));
	memset(begin, 0, sizeof(VALUE)*(end-begin));
	args->values = begin;
	args->size = end - begin;
	args->signature = sig;
}

size_t snow_arguments_size(SN_P p, const SnArguments* args) {
	return args->size + args->extra_size;
}

VALUE snow_argument_get(SN_P p, const SnArguments* args, int idx) {
	if (idx < args->size)
		return args->values[idx];
	idx -= args->size;
	if (idx < args->extra_size)
		return args->extra_values[idx];
	return NULL;
}

VALUE snow_argument_get_named(SN_P p, const SnArguments* args, SnSymbol name) {
	for (size_t i = 0; i < args->signature->num_params; ++i) {
		if (args->signature->params[i].name == name) {
			return args->values[i];
		}
	}
	for (size_t i = 0; i < args->extra_size; ++i) {
		if (args->extra_names[i] == name) {
			return args->extra_values[i];
		}
	}
	return NULL;
}

void snow_argument_push(SN_P p, SnArguments* args, VALUE val) {
	for (size_t i = 0; i < args->size; ++i) {
		if (args->values[i] == NULL) {
			args->values[i] = val;
			return;
		}
	}
}

void snow_argument_set_named(SN_P p, SnArguments* args, SnSymbol name, VALUE val) {
	for (size_t i = 0; i < args->signature->num_params; ++i) {
		if (args->signature->params[i].name == name) {
			args->values[i] = val;
			return;
		}
	}
	
	for (size_t i = 0; i < args->extra_size; ++i) {
		if (args->extra_names[i] == name) {
			args->extra_values[i] = val;
			return;
		}
	}
	
	size_t i = args->extra_size;
	args->extra_names = (SnSymbol*)realloc(args->extra_names, sizeof(SnSymbol)*(i+1));
	args->extra_values = (VALUE*)realloc(args->extra_values, sizeof(VALUE)*(i+1));
	args->extra_names[i] = name;
	args->extra_values[i] = val;
	args->extra_size++;
}
