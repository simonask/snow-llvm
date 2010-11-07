#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/vm.h"
#include "snow/function.h"
#include "snow/str.h"

SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnFunctionCallContext* definition_context) {
	SnFunction* obj = (SnFunction*)snow_gc_alloc_object(SnFunctionType);
	obj->descriptor = descriptor;
	obj->definition_context = definition_context;
	return obj;
}

SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args) {
	if (!callee->descriptor->needs_context) return callee->definition_context;
	
	SnFunctionCallContext* context = (SnFunctionCallContext*)snow_gc_alloc_object(SnFunctionCallContextType);
	context->function = callee;
	context->caller = caller;
	// TODO! Set arguments etc.
	return context;
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
	return function->descriptor->ptr(context, self, it);
}


SnFunction* snow_create_method(SnFunctionPtr ptr, size_t num_args) {
	SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)malloc(sizeof(SnFunctionDescriptor));
	descriptor->ptr = ptr;
	descriptor->return_type = SnAnyType;
	descriptor->num_params = num_args;
	descriptor->param_types = num_args ? (SnType*)malloc(sizeof(SnType)*num_args) : NULL;
	descriptor->param_names = num_args ? (SnSymbol*)malloc(sizeof(SnSymbol)*num_args) : NULL;
	descriptor->it_index = 0;
	descriptor->local_names = descriptor->param_names;
	descriptor->num_locals = num_args;
	descriptor->needs_context = num_args > 1 ? true : false;
	
	for (size_t i = 0; i < num_args; ++i) {
		descriptor->param_types[i] = SnAnyType; // TODO
		descriptor->param_names[i] = 0;
	}
	
	return snow_create_function(descriptor, NULL);
}


static VALUE function_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	char buffer[100];
	snprintf(buffer, 100, "[Function@%p]", self);
	return snow_create_string(buffer);
}

SnObject* snow_create_function_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", function_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", function_inspect, 0);
	return proto;
}
