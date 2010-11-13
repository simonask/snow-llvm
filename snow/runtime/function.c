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

static void ensure_extra_args(SnFunctionCallContext* context, size_t max_num_names, VALUE* named_args) {
	if (!context->extra) {
		context->extra = (SnFunctionCallContextExtraArguments*)malloc(sizeof(SnFunctionCallContextExtraArguments));
		context->extra->names = max_num_names ? (SnSymbol*)malloc(sizeof(SnSymbol)*max_num_names) : NULL; // heuristic maximum
		context->extra->named_args = named_args;
		context->extra->unnamed_args = NULL; // unknowable at this point
		context->extra->num_names = context->extra->num_args = 0; // unknowable at this point
	}
}

SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args) {
	ASSERT(num_names <= num_args);
	if (!callee->descriptor->needs_context) return callee->definition_context;
	
	SnFunctionCallContext* context = SN_GC_ALLOC_OBJECT(SnFunctionCallContext);
	context->function = callee;
	context->caller = caller;
	context->self = NULL; // set in snow_function_call
	context->extra = NULL;
	
	const SnFunctionDescriptor* descriptor = callee->descriptor;
	
	size_t num_params = descriptor->num_params;
	
	// allocate space for more locals than needed, to avoid multiple allocations and ensure linear runtime
	// TODO: Improve this heuristic.
	size_t max_num_locals = descriptor->num_locals + num_args;
	
	VALUE* locals = (VALUE*)malloc(sizeof(VALUE) * max_num_locals);
	memset(locals, 0, sizeof(VALUE) * max_num_locals);
	context->locals = locals;
	
	VALUE* extra_named_args = context->locals + descriptor->num_locals;
	// extra_unnamed_args depends on the number of extra named args, which we don't know yet
	
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
				locals[param_i] = args[arg_i];
				++arg_i;
				break;
			} else {
				// named argument, but not part of the function definition.
				// Add it to the extra args.
				ensure_extra_args(context, num_names, extra_named_args);
				
				extra_named_args[num_extra_names] = args[arg_i];
				context->extra->names[num_extra_names] = names[arg_i];
				++num_extra_names;
				++num_extra_args;
				++arg_i;
			}
		}
		
		if (!found) {
			// take an unnamed arg
			locals[param_i] = args[arg_i];
			++arg_i;
		}
		
		++param_i;
	}
	
	// Extra named args
	if (arg_i < num_args)
		ensure_extra_args(context, num_names, extra_named_args);
	while (arg_i < num_names) {
		context->extra->names[num_extra_names] = names[arg_i];
		context->extra->named_args[num_extra_args] = args[arg_i];
		++arg_i;
		++num_extra_names;
		++num_extra_args;
	}
	// Extra unnamed args
	if (arg_i < num_args) {
		context->extra->num_names = num_extra_names;
		context->extra->unnamed_args = context->extra->named_args + num_extra_names;
	}
	while (arg_i < num_args) {
		context->extra->unnamed_args[num_extra_args++] = args[arg_i++];
		context->extra->num_args = num_extra_args;
	}
	
	// PHEW! The worst is over.
	
	/*printf("locals array:\n");
	for (size_t i = 0; i < max_num_locals; ++i) {
		if (i < descriptor->num_params) {
			printf("%s\t", snow_sym_to_cstr(descriptor->param_names[i]));
		} else {
			printf("\t\t");
		}
		printf("%lu:\t%p\n", i, context->locals[i]);
	}*/
	
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
	if (context->extra) {
		free(context->extra->names);
		free(context->extra);
	}
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
