#include "snow/arguments.h"
#include "snow/gc.h"

SnArguments* snow_create_arguments(size_t num_names, size_t size) {
	SnArguments* args = SN_GC_ALLOC_OBJECT(SnArguments);
	args->num_extra_names = num_names;
	args->extra_names = num_names ? (SnSymbol*)malloc(sizeof(SnSymbol)*num_names) : NULL;
	args->size = size;
	args->data = size ? (VALUE*)malloc(sizeof(VALUE)*size) : NULL;
	memset(args->extra_names, -1, num_names*sizeof(SnSymbol));
	memset(args->data, 0, size*sizeof(VALUE));
	return args;
}

void snow_arguments_grow_by(SnArguments* args, size_t num_names, size_t size) {
	if (num_names) {
		args->extra_names = (SnSymbol*)realloc(args->extra_names, (args->num_extra_names+num_names)*sizeof(SnSymbol));
		memset(args->extra_names + args->num_extra_names, -1, num_names*sizeof(SnSymbol));
		args->num_extra_names += num_names;
	}
	if (size) {
		args->data = (VALUE*)realloc(args->data, (args->size+size)*sizeof(VALUE));
		memset(args->data + args->size, 0, size*sizeof(VALUE));
		args->size += size;
	}
}