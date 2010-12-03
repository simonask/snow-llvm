#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/vm.h"
#include "snow/function.h"
#include "snow/str.h"

#include <string.h>

SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnFunctionCallContext* definition_context) {
	SnFunction* obj = SN_GC_ALLOC_OBJECT(SnFunction);
	obj->descriptor = descriptor;
	obj->definition_context = definition_context;
	return obj;
}

SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args) {
	ASSERT(num_names <= num_args);
	if (!callee->descriptor->needs_context) return callee->definition_context;
	
	SnFunctionCallContext* context = SN_GC_ALLOC_OBJECT(SnFunctionCallContext);
	context->function = callee;
	context->caller = caller;
	context->self = NULL; // set in snow_function_call
	const SnFunctionDescriptor* descriptor = callee->descriptor;
	
	num_args = num_args < descriptor->num_params ? descriptor->num_params : num_args;
	size_t num_locals = descriptor->num_locals;
	VALUE* locals = num_locals ? (VALUE*)malloc(sizeof(VALUE) * num_locals) : NULL;
	memset(locals, 0, sizeof(VALUE) * num_locals);
	context->locals = locals;
	
	VALUE* param_args = (VALUE*)alloca(descriptor->num_params * sizeof(SnSymbol));
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
		while (arg_i < num_names) {
			if (names[arg_i] == descriptor->param_names[param_i]) {
				found = true;
				param_args[param_i] = args[arg_i];
				++arg_i;
				break;
			} else {
				// named argument, but not part of the function definition.
				
				extra_args[num_extra_names] = args[arg_i];
				extra_names[num_extra_names] = names[arg_i];
				++num_extra_names;
				++num_extra_args;
				++arg_i;
			}
		}
		
		if (!found) {
			// take an unnamed arg
			param_args[param_i] = args[arg_i];
			++arg_i;
		}
		
		++param_i;
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
	
	context->arguments = snow_create_arguments(num_extra_names, num_args);
	context->arguments->descriptor = descriptor;
	memcpy(context->arguments->data, param_args, descriptor->num_params*sizeof(VALUE));
	memcpy(context->arguments->data + descriptor->num_params, extra_args, num_extra_args*sizeof(VALUE));
	memcpy(context->arguments->extra_names, extra_names, num_extra_names*sizeof(SnSymbol));
	
	// PHEW! The worst is over.
	
	return context;
}

void snow_merge_splat_arguments(SnFunctionCallContext* callee_context, VALUE merge_in)
{
	SnType type = snow_type_of(merge_in);
	switch (type) {
		case SnArrayType: {
			SnArray* array = (SnArray*)merge_in;
			size_t n = snow_array_size(array);
			if (n) {
				
			}
			break;
		}
		case SnMapType: {
			break;
		}
		default: {
			fprintf(stderr, "WARNING: Splat argument '%p' is not a map or array.\n", merge_in);
			break;
		}
	}
}

SnFunction* snow_value_to_function(VALUE val) {
	// TODO: Functor support
	ASSERT(snow_type_of(val) == SnFunctionType);
	return (SnFunction*)val;
}

void snow_finalize_function(SnFunction* func) {
}

void snow_finalize_function_call_context(SnFunctionCallContext* context) {
	free(context->locals);
}

VALUE snow_function_call(SnFunction* function, SnFunctionCallContext* context, VALUE self, VALUE it) {
	//printf("snow_function_call(function: %p(%s), context: %p, self: %p, it: %p)\n", function, "<unnamed>"/*snow_string_cstr(snow_vm_get_name_of(function->descriptor->ptr))*/, context, self, it);
	if (!self)
		self = function->definition_context ? function->definition_context->self : NULL;
	if (function->descriptor->needs_context) {
		// context is owned by function
		context->self = self;
	}
	return function->descriptor->ptr(context, self, it);
}


SnFunction* snow_create_method(SnFunctionPtr ptr, int num_args) {
	SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)malloc(sizeof(SnFunctionDescriptor));
	descriptor->ptr = ptr;
	descriptor->return_type = SnAnyType;
	descriptor->num_params = num_args > 0 ? num_args : 0;
	descriptor->param_types = num_args > 0 ? (SnType*)malloc(sizeof(SnType)*num_args) : NULL;
	descriptor->param_names = num_args > 0 ? (SnSymbol*)malloc(sizeof(SnSymbol)*num_args) : NULL;
	descriptor->it_index = 0;
	descriptor->local_names = descriptor->param_names;
	descriptor->num_locals = num_args > 0 ? num_args : 0;
	descriptor->needs_context = num_args < 0 || num_args > 1;
	
	for (int i = 0; i < num_args; ++i) {
		descriptor->param_types[i] = SnAnyType; // TODO
		descriptor->param_names[i] = (uint64_t)-1;
	}
	
	return snow_create_function(descriptor, NULL);
}


static VALUE function_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	char buffer[100];
	snprintf(buffer, 100, "[Function@%p(%s)]", self, /*snow_string_cstr(snow_vm_get_name_of(here->function->descriptor->ptr))*/ "<name>");
	return snow_create_string(buffer);
}

SnObject* snow_create_function_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", function_inspect, -1);
	SN_DEFINE_METHOD(proto, "to_string", function_inspect, -1);
	return proto;
}
