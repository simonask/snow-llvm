#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/vm.h"
#include "snow/function.h"

SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnFunctionCallContext* definition_context) {
	SnFunction* obj = (SnFunction*)snow_gc_alloc_object(SnFunctionType);
	obj->descriptor = descriptor;
	obj->definition_context = definition_context;
	return obj;
}

SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
	if (!callee->descriptor->needs_context) return caller;
	
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