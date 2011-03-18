#include "snow/arguments.h"
#include "snow/gc.h"
#include "snow/object.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/snow.h"
#include "snow/array.h"
#include "snow/map.h"

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

SnArguments* snow_create_arguments_for_function_call(const SnFunctionDescriptor* descriptor, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args)
{
	VALUE* param_args = (VALUE*)alloca(descriptor->num_params * sizeof(SnSymbol));
	memset(param_args, 0, descriptor->num_params * sizeof(SnSymbol));
	SnSymbol* extra_names = (SnSymbol*)alloca(num_names*sizeof(SnSymbol));
	VALUE* extra_args = (VALUE*)alloca(num_args*sizeof(VALUE));
	
	size_t num_extra_names = 0;
	size_t num_extra_args = 0;
	size_t first_unnamed_arg = num_names;
	
	// first, put named args in place
	// INFO: The following code is why param names and named arguments are sorted by symbol value.
	size_t param_i = 0;
	size_t arg_i = 0;
	while (param_i < descriptor->num_params) {
		bool found = false;
		while (arg_i < num_names && arg_i < num_args) {
			if (names[arg_i] == descriptor->param_names[param_i]) {
				found = true;
				param_args[param_i++] = args[arg_i++];
				break;
			} else {
				// named argument, but not part of the function definition.
				extra_args[num_extra_names++] = args[arg_i];
				extra_names[num_extra_names++] = names[arg_i];
				++arg_i;
			}
		}
		
		if (!found) {
			// take an unnamed arg
			if (arg_i < num_args) {
				param_args[param_i++] = args[arg_i++];
			} else {
				param_args[param_i++] = NULL;
			}
		}
	}
	
	// Extra named args
	if (arg_i < num_names) {
		memcpy(extra_names + num_extra_names, names + arg_i, (num_names - arg_i)*sizeof(SnSymbol));
		memcpy(extra_args + num_extra_args, args + arg_i, (num_names - arg_i)*sizeof(VALUE));
		num_extra_names += num_names - arg_i;
		num_extra_args += num_names - arg_i;
		arg_i = num_names;
	}
	if (arg_i < num_args) {
		memcpy(extra_args + num_extra_args, args + arg_i, (num_args - arg_i)*sizeof(VALUE));
		num_extra_args += num_args - arg_i;
		arg_i = num_args;
	}
	
	size_t num_args_upper_bound = descriptor->num_params > num_args ? descriptor->num_params : num_args;
	SnArguments* arguments = snow_create_arguments(num_extra_names, num_args_upper_bound);
	arguments->descriptor = descriptor;
	memcpy(arguments->data, param_args, param_i*sizeof(VALUE));
	memcpy(arguments->data + descriptor->num_params, extra_args, num_extra_args*sizeof(VALUE));
	memcpy(arguments->extra_names, extra_names, num_extra_names*sizeof(SnSymbol));
	return arguments;
}

static void arguments_grow_by_unlocked(SnArguments* args, size_t num_names, size_t size) {
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

void snow_arguments_grow_by(SnArguments* args, size_t num_names, size_t size) {
	SN_GC_WRLOCK(args);
	arguments_grow_by_unlocked(args, num_names, size);
	SN_GC_UNLOCK(args);
}

VALUE snow_arguments_get_by_name(SnArguments* args, SnSymbol name) {
	// TODO: Since argument names are always in order, consider using bsearch here instead.
	SN_GC_RDLOCK(args);
	VALUE val = NULL;
	
	size_t offset = 0;
	if (args->descriptor) {
		for (size_t i = 0; i < args->descriptor->num_params; ++i) {
			if (name == args->descriptor->param_names[i]) {
				val = args->data[i];
				goto out;
			}
		}
		offset = args->descriptor->num_params;
	}
	
	for (size_t i = 0; i < args->num_extra_names; ++i) {
		if (name == args->extra_names[i]) {
			val = args->data[offset + i];
			goto out;
		}
	}
	
out:
	SN_GC_UNLOCK(args);
	return val;
}

static void arguments_append_values(SnArguments* args, size_t num_values, const VALUE* values) {
	SN_GC_WRLOCK(args);
	size_t offset = 0;
	for (size_t i = 0; (i < args->size) && (offset < num_values); ++i) {
		if (!args->data[i]) {
			args->data[i] = values[offset++];
		}
	}
	size_t old_size = args->size;
	arguments_grow_by_unlocked(args, 0, num_values - offset);
	memcpy(args->data + old_size, values + offset, (num_values-offset)*sizeof(VALUE));
	SN_GC_UNLOCK(args);
}

void snow_arguments_append_array(SnArguments* args, const SnArray* array) {
	size_t n = snow_array_size(array);
	VALUE* values = (VALUE*)alloca(n*sizeof(VALUE));
	n = snow_array_get_all(array, values, n);
	if (n) arguments_append_values(args, n, values);
}

void snow_arguments_append_map(SnArguments* args, const SnMap* map) {
	// XXX: TODO!!
}

void snow_arguments_merge(SnArguments* args, const SnArguments* other) {
	// TODO: Merge named arguments
	SN_GC_RDLOCK(other);
	size_t sz = other->size;
	VALUE* values = (VALUE*)alloca(sz*sizeof(VALUE));
	memcpy(values, other->data, sz*sizeof(VALUE));
	SN_GC_UNLOCK(other);
	
	arguments_append_values(args, sz, values);
}

static VALUE arguments_inspect(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnArgumentsType) return NULL;
	SnArguments* args = (SnArguments*)self;
	
	SnString* result = snow_string_format("[Arguments@%p (", self);
	
	for (size_t i = 0; i < args->descriptor->num_params; ++i) {
		snow_string_append_cstr(result, snow_sym_to_cstr(args->descriptor->param_names[i]));
		snow_string_append_cstr(result, ": ");
		snow_string_append(result, snow_value_inspect(args->data[i]));
		if (i != args->size-1) snow_string_append_cstr(result, ", ");
	}
	
	for (size_t i = 0; i < args->num_extra_names; ++i) {
		snow_string_append_cstr(result, snow_sym_to_cstr(args->extra_names[i]));
		snow_string_append_cstr(result, ": ");
		snow_string_append(result, snow_value_inspect(args->data[args->descriptor->num_params+i]));
		if (i != args->size-1) snow_string_append_cstr(result, ", ");
	}
	
	for (size_t i = args->descriptor->num_params + args->num_extra_names; i < args->size; ++i) {
		snow_string_append(result, snow_value_inspect(args->data[i]));
		if (i != args->size-1) snow_string_append_cstr(result, ", ");
	}
	
	snow_string_append_cstr(result, ")]");
	
	return result;
}

static VALUE arguments_splat(SnCallFrame* here, VALUE self, VALUE it) {
	return self;
}

SnObject* snow_create_arguments_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "*", arguments_splat, 0);
	SN_DEFINE_METHOD(proto, "inspect", arguments_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", arguments_inspect, 0);
	return proto;
}