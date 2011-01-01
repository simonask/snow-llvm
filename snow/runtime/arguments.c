#include "snow/arguments.h"
#include "snow/gc.h"
#include "snow/object.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/snow.h"

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

static VALUE arguments_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnArgumentsType) return NULL;
	SnArguments* args = (SnArguments*)self;
	
	SnString* result = snow_string_format("[Arguments@%p (", self);
	
	for (size_t i = 0; i < args->descriptor->num_params; ++i) {
		snow_string_append_cstr(result, snow_sym_to_cstr(args->descriptor->param_names[i]));
		snow_string_append_cstr(result, ": ");
		VALUE arg = args->data[i];
		VALUE inspected = SN_CALL_METHOD(arg, "inspect", 0, NULL);
		if (snow_type_of(inspected) == SnStringType) {
			snow_string_append(result, (SnString*)inspected);
		}
		
		if (i != args->size-1) {
			snow_string_append_cstr(result, ", ");
		}
	}
	
	for (size_t i = 0; i < args->num_extra_names; ++i) {
		snow_string_append_cstr(result, snow_sym_to_cstr(args->extra_names[i]));
		snow_string_append_cstr(result, ": ");
		VALUE arg = args->data[args->descriptor->num_params + i];
		VALUE inspected = SN_CALL_METHOD(arg, "inspect", 0, NULL);
		if (snow_type_of(inspected) == SnStringType) {
			snow_string_append(result, (SnString*)inspected);
		}
		
		if (i != args->size-1) {
			snow_string_append_cstr(result, ", ");
		}
	}
	
	for (size_t i = args->descriptor->num_params + args->num_extra_names; i < args->size; ++i) {
		VALUE arg = args->data[i];
		VALUE inspected = SN_CALL_METHOD(arg, "inspect", 0, NULL);
		if (snow_type_of(inspected) == SnStringType) {
			snow_string_append(result, (SnString*)inspected);
		}
		
		if (i != args->size-1) {
			snow_string_append_cstr(result, ", ");
		}
	}
	
	snow_string_append_cstr(result, ")]");
	
	return result;
}

static VALUE arguments_splat(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return self;
}

SnObject* snow_create_arguments_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "*", arguments_splat, 0);
	SN_DEFINE_METHOD(proto, "inspect", arguments_inspect, 0);
	return proto;
}