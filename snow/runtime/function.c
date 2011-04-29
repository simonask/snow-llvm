#include "snow/function.h"
#include "internal.h"
#include "snow/class.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/map.h"
#include "snow/gc.h"
#include "snow/vm.h"
#include "snow/str.h"
#include "snow/snow.h"
#include "snow/exception.h"

#include <string.h>

SnFunction* snow_create_function(const SnFunctionDescriptor* descriptor, SnCallFrame* definition_context) {
	SnFunction* obj = SN_GC_ALLOC_OBJECT(SnFunction);
	SN_GC_WRLOCK(obj);
	obj->descriptor = descriptor;
	obj->definition_context = definition_context;
	obj->variable_references = NULL;
	if (descriptor->num_variable_references) {
		obj->variable_references = (VALUE**)malloc(sizeof(VALUE*)*descriptor->num_variable_references);
		
		for (size_t i = 0; i < descriptor->num_variable_references; ++i) {
			SnVariableReference* ref = descriptor->variable_references + i;
			SnCallFrame* ref_context = definition_context;
			for (int level = 1; level < ref->level; ++level) {
				ref_context = ref_context->function->definition_context;
				ASSERT(ref_context && "Invalid variable reference! Function descriptor is corrupt.");
			}
			ASSERT(ref->level > 0 && ref->index < ref_context->function->descriptor->num_locals && "Invalid variable reference index! Function descriptor is corrupt.");
			obj->variable_references[i] = ref_context->locals + ref->index;
		}
	}
	SN_GC_UNLOCK(obj);
	return obj;
}

SnCallFrame* snow_create_call_frame(SnFunction* callee, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args) {
	if (!callee->descriptor->needs_context) return callee->definition_context;
	
	SN_GC_RDLOCK(callee);
	const SnFunctionDescriptor* descriptor = callee->descriptor;
	SN_GC_UNLOCK(callee);
	
	SnArguments* arguments = snow_create_arguments_for_function_call(descriptor, num_names, names, num_args, args);
	
	SnCallFrame* context = SN_GC_ALLOC_OBJECT(SnCallFrame);
	SN_GC_WRLOCK(context);
	context->function = callee;
	context->caller = NULL;
	context->self = NULL; // set in snow_function_call
	context->module = callee->definition_context ? callee->definition_context->module : NULL;
	context->arguments = arguments;
	
	// arguments are also the first locals
	const size_t num_locals = descriptor->num_locals;
	context->locals = num_locals ? (VALUE*)malloc(sizeof(VALUE)*num_locals) : NULL;
	memset(context->locals, 0, sizeof(VALUE)*num_locals);
	size_t num_set_arguments = context->arguments->size > descriptor->num_params ? descriptor->num_params : context->arguments->size;
	memcpy(context->locals, context->arguments->data, sizeof(VALUE)*num_set_arguments);
	SN_GC_UNLOCK(context);
	
	return context;
}

struct named_arg { SnSymbol name; VALUE value; };
static int named_arg_cmp(const void* _a, const void* _b) {
	const struct named_arg* a = (struct named_arg*)_a;
	const struct named_arg* b = (struct named_arg*)_b;
	return (int)((int64_t)b->name - (int64_t)a->name);
}

void snow_merge_splat_arguments(SnCallFrame* callee_context, VALUE merge_in) {
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
			snow_throw_exception_with_description("Splat argument '%s' is not a map or an array.", snow_value_inspect_cstr(merge_in));
			return;
		}
	}
	
	// Set the locals representing the arguments
	const SnFunctionDescriptor* descriptor = callee_context->function->descriptor;
	for (size_t i = 0; (i < descriptor->num_locals) && (i < args->size); ++i) {
		if (!callee_context->locals[i])
			callee_context->locals[i] = args->data[i];
	}
}

SnFunction* snow_value_to_function(VALUE val, VALUE* in_out_new_self) {
	VALUE functor = val;
	while (functor && snow_type_of(functor) != SnFunctionType) {
		*in_out_new_self = functor;
		functor = snow_get_member(functor, snow_sym("__call__"));
	}
	
	if (!functor) {
		snow_throw_exception_with_description("'%s' is not a function, and does not respond to __call__.", snow_value_inspect_cstr(val));
		return NULL; // unreachable
	}
	
	return (SnFunction*)functor;
}

void snow_finalize_function(SnFunction* func) {
	free(func->variable_references);
}

void snow_finalize_call_frame(SnCallFrame* context) {
	free(context->locals);
}

VALUE snow_function_call(SnFunction* function, SnCallFrame* context, VALUE self, VALUE it) {
	ASSERT(snow_type_of(function) == SnFunctionType);
	if (!self)
		self = function->definition_context ? function->definition_context->self : NULL;
	if (function->descriptor->needs_context) {
		// context is owned by function, don't change it if we don't own it.
		context->self = self;
	}
	return function->descriptor->ptr(function, context, self, it);
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
	descriptor->jit_info = NULL;
	descriptor->num_variable_references = 0;
	descriptor->variable_references = NULL;
	
	for (int i = 0; i < num_args; ++i) {
		descriptor->param_types[i] = SnAnyType; // TODO
		descriptor->param_names[i] = (uint64_t)-1;
	}
	
	return snow_create_function(descriptor, NULL);
}


static VALUE function_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) == SnFunctionType) {
		SnFunction* f = (SnFunction*)self;
		return snow_string_format("[Function@%p(%s)]", self, snow_sym_to_cstr(f->descriptor->name));
	}
	return snow_string_format("[Function@%p]", self);
}

static VALUE function_name(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) == SnFunctionType) {
		SnFunction* f = (SnFunction*)self;
		return snow_symbol_to_value(f->descriptor->name);
	}
	return NULL;
}

SnClass* snow_get_function_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("inspect", function_inspect, 0),
			SN_METHOD("to_string", function_inspect, 0),
			SN_PROPERTY("name", function_name, NULL),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Function"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}

static VALUE call_frame_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnCallFrameType) return NULL;
	SnCallFrame* context = (SnCallFrame*)self;
	SnString* result = snow_string_format("[CallFrame@%p function:%p(%s) locals:(", context, context->function, snow_sym_to_cstr(context->function->descriptor->name));
	for (size_t i = 0; i < context->function->descriptor->num_locals; ++i) {
		snow_string_append_cstr(result, snow_sym_to_cstr(context->function->descriptor->local_names[i]));
		snow_string_append_cstr(result, " => ");
		snow_string_append(result, snow_value_inspect(context->locals[i]));
		if (i != context->function->descriptor->num_locals-1) snow_string_append_cstr(result, ", ");
	}
	snow_string_append_cstr(result, ")]");
	return result;
}

static VALUE call_frame_get_arguments(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnCallFrameType);
	SnCallFrame* context = (SnCallFrame*)self;
	return context->arguments;
}

static VALUE call_frame_get_locals(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnCallFrameType) return NULL;
	SnCallFrame* context = (SnCallFrame*)self;
	SnMap* map = snow_create_map_with_immediate_keys_and_insertion_order();
	const SnFunctionDescriptor* descriptor = context->function->descriptor;
	for (size_t i = 0; i < descriptor->num_locals; ++i) {
		snow_map_set(map, snow_symbol_to_value(descriptor->local_names[i]), context->locals[i]);
	}
	return map;
}

static VALUE call_frame_get_caller(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnCallFrameType) return NULL;
	return ((SnCallFrame*)self)->caller;
}

SnClass* snow_get_call_frame_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("inspect", call_frame_inspect, 0),
			SN_PROPERTY("arguments", call_frame_get_arguments, NULL),
			SN_PROPERTY("locals", call_frame_get_locals, NULL),
			SN_PROPERTY("caller", call_frame_get_caller, NULL)
		};
		SnClass* cls = snow_define_class(snow_sym("CallFrame"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}
