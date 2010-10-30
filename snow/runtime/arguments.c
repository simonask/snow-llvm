#include "snow/arguments.h"
#include "snow/function.h"

#include <string.h>
#include <stdlib.h>

void _snow_arguments_get_size_for_function(SnArguments* args, uint32_t req_size, SnFunction* function) {
	args->signature = function->descriptor->signature;
	size_t arity = args->signature->num_params;
	args->size = req_size > arity ? req_size : arity;
}

void _snow_arguments_init(SnArguments* args) {
	int32_t known_named = args->signature->num_params;
	memset(args->names + known_named, 0, args->size - known_named);
	for (int32_t i = 0; i < known_named; ++i) {
		args->names[i] = snow_symbol_to_value(args->signature->param_names[i]);
	}
	memset(args->values, 0, args->size * sizeof(VALUE));
}

void snow_arguments_set_named(SnArguments* args, SnSymbol name, VALUE val) {
	VALUE vsym = snow_symbol_to_value(name);
	for (size_t i = 0; i < args->size; ++i) {
		if (args->names[i] == vsym) {
			args->values[i] = val;
			return;
		}
	}
	
	for (size_t i = args->signature->num_params; i < args->size; ++i) {
		if (args->names[i] == NULL && args->values[i] == NULL) {
			args->names[i] = vsym;
			args->values[i] = val;
			return;
		}
	}
	
	TRAP(); // Out of room in Arguments vector.
}

void snow_arguments_push(SnArguments* args, VALUE val) {
	for (size_t i = 0; i < args->size; ++i) {
		if (args->values[i] == NULL) {
			args->values[i] = val;
			return;
		}
	}
	
	TRAP(); // Out of room in Arguments vector.
}

VALUE snow_arguments_get_named(const SnArguments* args, SnSymbol name) {
	VALUE vsym = snow_symbol_to_value(name);
	for (size_t i = 0; i < args->size; ++i) {
		if (args->names[i] == vsym) return args->values[i];
	}
	return NULL;
}

VALUE snow_arguments_get(const SnArguments* args, uint32_t idx) {
	if (idx > args->size) return NULL;
	return args->values[idx];
}

size_t snow_arguments_size(const SnArguments* args) {
	return args->size;
}
