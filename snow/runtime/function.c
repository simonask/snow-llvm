#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/map.h"
#include "snow/gc.h"
#include "snow/vm.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/snow.h"

#include <string.h>

SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnFunctionCallContext* definition_context) {
	SnFunction* obj = SN_GC_ALLOC_OBJECT(SnFunction);
	obj->descriptor = descriptor;
	obj->definition_context = definition_context;
	obj->variable_references = (VALUE**)0x1;
	return obj;
}

SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args) {
	if (!callee->descriptor->needs_context) return callee->definition_context;
	
	const SnFunctionDescriptor* descriptor = callee->descriptor;
	
	SnFunctionCallContext* context = SN_GC_ALLOC_OBJECT(SnFunctionCallContext);
	context->function = callee;
	context->caller = caller;
	context->self = NULL; // set in snow_function_call
	context->module = callee->definition_context ? callee->definition_context->module : NULL;
	context->arguments = snow_create_arguments_for_function_call(descriptor, num_names, names, num_args, args);
	
	// arguments are also the first locals
	const size_t num_locals = descriptor->num_locals;
	context->locals = num_locals ? (VALUE*)malloc(sizeof(VALUE)*num_locals) : NULL;
	memset(context->locals, 0, sizeof(VALUE)*num_locals);
	size_t num_set_arguments = context->arguments->size > descriptor->num_params ? descriptor->num_params : context->arguments->size;
	memcpy(context->locals, context->arguments->data, sizeof(VALUE)*num_set_arguments);
	
	return context;
}

struct named_arg { SnSymbol name; VALUE value; };
static int named_arg_cmp(const void* _a, const void* _b) {
	const struct named_arg* a = (struct named_arg*)_a;
	const struct named_arg* b = (struct named_arg*)_b;
	return (int)((int64_t)b->name - (int64_t)a->name);
}

void snow_merge_splat_arguments(SnFunctionCallContext* callee_context, VALUE merge_in) {
	SnArguments* args = callee_context->arguments;
	switch (snow_type_of(merge_in)) {
		case SnArrayType:
			snow_arguments_append_array(args, (SnArray*)merge_in);
			break;
		case SnMapType:
			snow_arguments_append_map(args, (SnMap*)merge_in);
			break;
		case SnArgumentsType:
			snow_arguments_merge(args, (SnArguments*)merge_in);
			break;
		default: {
			fprintf(stderr, "WARNING: Splat argument '%p' is not a map or array.\n", merge_in);
			break;
		}
	}
	
	// Set the locals representing the arguments
	const SnFunctionDescriptor* descriptor = callee_context->function->descriptor;
	for (size_t i = 0; (i < descriptor->num_locals) && (i < args->size); ++i) {
		if (!callee_context->locals[i])
			callee_context->locals[i] = args->data[i];
	}
}

SnFunction* snow_value_to_function(VALUE val) {
	VALUE functor = val;
	while (functor && snow_type_of(functor) != SnFunctionType) {
		functor = snow_get_member(functor, snow_sym("__call__"));
	}
	
	if (!functor) {
		fprintf(stderr, "ERROR: %p is not a function, and doesn't respond to __call__.\n", val);
		TRAP(); // TODO: Exception
	}
	
	return (SnFunction*)functor;
}

VALUE snow_get_local(SnFunctionCallContext* here, int adjusted_level, int index) {
	SnFunctionCallContext* ctx = here;
	for (int i = 0; i < adjusted_level; ++i) {
		ctx = ctx->function->definition_context;
	}
	
	ASSERT(ctx);
	ASSERT(index < ctx->function->descriptor->num_locals);
	return ctx->locals[index];
}

VALUE snow_set_local(SnFunctionCallContext* here, int adjusted_level, int index, VALUE value) {
	SnFunctionCallContext* ctx = here;
	for (int i = 0; i < adjusted_level; ++i) {
		ctx = ctx->function->definition_context;
	}
	
	ASSERT(ctx);
	ASSERT(index < ctx->function->descriptor->num_locals);
	ctx->locals[index] = value;
	return value;
}

void snow_finalize_function(SnFunction* func) {
}

void snow_finalize_function_call_context(SnFunctionCallContext* context) {
	free(context->locals);
}

VALUE snow_function_call(SnFunction* function, SnFunctionCallContext* context, VALUE self, VALUE it) {
	ASSERT(snow_type_of(function) == SnFunctionType);
	if (!self)
		self = function->definition_context ? function->definition_context->self : NULL;
	if (function->descriptor->needs_context) {
		// context is owned by function
		context->self = self;
	}
	return function->descriptor->ptr(context, self, it);
}


SnFunction* snow_create_method(SnFunctionPtr ptr, SnSymbol name, int num_args) {
	SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)malloc(sizeof(SnFunctionDescriptor));
	descriptor->ptr = ptr;
	descriptor->name = name;
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
	if (snow_type_of(self) == SnFunctionType) {
		SnFunction* function = (SnFunction*)self;
		return snow_string_format("[Function@%p(%s)]", self, snow_sym_to_cstr(function->descriptor->name));
	}
	return snow_string_format("[Function@%p]", self);
}

static VALUE function_name(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) == SnFunctionType) {
		SnFunction* function = (SnFunction*)self;
		return snow_symbol_to_value(function->descriptor->name);
	}
	return NULL;
}

SnObject* snow_create_function_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", function_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", function_inspect, 0);
	SN_DEFINE_PROPERTY(proto, "name", function_name, 0);
	return proto;
}

static VALUE function_call_context_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFunctionCallContextType) return NULL;
	SnFunctionCallContext* context = (SnFunctionCallContext*)self;
	SnString* result = snow_string_format("[FunctionCallContext@%p function:%p(%s) locals:(", context, context->function, snow_sym_to_cstr(context->function->descriptor->name));
	for (size_t i = 0; i < context->function->descriptor->num_locals; ++i) {
		snow_string_append_cstr(result, snow_sym_to_cstr(context->function->descriptor->local_names[i]));
		snow_string_append_cstr(result, " => ");
		snow_string_append(result, snow_value_inspect(context->locals[i]));
		if (i != context->function->descriptor->num_locals-1) snow_string_append_cstr(result, ", ");
	}
	snow_string_append_cstr(result, ")]");
	return result;
}

static VALUE function_call_context_get_arguments(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnFunctionCallContextType);
	SnFunctionCallContext* context = (SnFunctionCallContext*)self;
	return context->arguments;
}

static VALUE function_call_context_get_locals(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFunctionCallContextType) return NULL;
	SnFunctionCallContext* context = (SnFunctionCallContext*)self;
	SnMap* map = snow_create_map_with_immediate_keys_and_insertion_order();
	const SnFunctionDescriptor* descriptor = context->function->descriptor;
	for (size_t i = 0; i < descriptor->num_locals; ++i) {
		snow_map_set(map, snow_symbol_to_value(descriptor->local_names[i]), context->locals[i]);
	}
	return map;
}

SnObject* snow_create_function_call_context_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", function_call_context_inspect, 0);
	SN_DEFINE_PROPERTY(proto, "arguments", function_call_context_get_arguments, NULL);
	SN_DEFINE_PROPERTY(proto, "locals", function_call_context_get_locals, NULL);
	return proto;
}
