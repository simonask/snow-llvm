#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
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
	return obj;
}

SnFunctionCallContext* snow_create_function_call_context(SnFunction* callee, SnFunctionCallContext* caller, size_t num_names, const SnSymbol* names, size_t num_args, const VALUE* args) {
	if (!callee->descriptor->needs_context) return callee->definition_context;
	
	SnFunctionCallContext* context = SN_GC_ALLOC_OBJECT(SnFunctionCallContext);
	context->function = callee;
	context->caller = caller;
	context->self = NULL; // set in snow_function_call
	context->module = callee->definition_context ? callee->definition_context->module : NULL;
	const SnFunctionDescriptor* descriptor = callee->descriptor;
	
	size_t num_locals = descriptor->num_locals;
	VALUE* locals = num_locals ? (VALUE*)malloc(sizeof(VALUE) * num_locals) : NULL;
	bzero(locals, sizeof(VALUE) * num_locals);
	context->locals = locals;
	
	VALUE* param_args = (VALUE*)alloca(descriptor->num_params * sizeof(SnSymbol));
	bzero(param_args, descriptor->num_params * sizeof(SnSymbol));
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
	
	context->arguments = snow_create_arguments(num_extra_names, num_args);
	context->arguments->descriptor = descriptor;
	memcpy(locals, param_args, param_i*sizeof(VALUE));
	memcpy(context->arguments->data, param_args, param_i*sizeof(VALUE));
	memcpy(context->arguments->data + descriptor->num_params, extra_args, num_extra_args*sizeof(VALUE));
	memcpy(context->arguments->extra_names, extra_names, num_extra_names*sizeof(SnSymbol));
	
	// PHEW! It's over.
	
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
		case SnArrayType: {
			SnArray* array = (SnArray*)merge_in;
			size_t n = snow_array_size(array);
			if (n) {
				size_t offset = 0;
				for (size_t i = 0; (i < args->size) && (offset < n); ++i) {
					if (!args->data[i]) {
						args->data[i] = array->data[offset++];
					}
				}
				size_t old_size = args->size;
				snow_arguments_grow_by(args, 0, n - offset);
				memcpy(args->data + old_size, array->data + offset, (n-offset)*sizeof(VALUE));
			}
			break;
		}
		case SnMapType: {
			// TODO!!
			break;
		}
		case SnArgumentsType: {
			SnArguments* src = (SnArguments*)merge_in;
			
			// Warning: This gets complicated fast. Hold tight.
			// The following is similar to, but *not quite* the same as,
			// the function snow_create_function_call_context above.
			struct named_arg* extra_named_args = (struct named_arg*)alloca((src->size + args->num_extra_names)*sizeof(struct named_arg));
			size_t num_extra_named_args = 0;
			VALUE* extra_unnamed_args = (VALUE*)alloca((src->size + args->size)*sizeof(VALUE));
			size_t num_extra_unnamed_args = 0;
			
			// First, check the target arguments' described named args against the source's
			// described named args.
			fprintf(stderr, "Merge SnArguments, step 1\n");
			size_t param_i = 0;
			size_t src_param_i = 0;
			size_t src_unnamed_i = src->descriptor->num_params + src->num_extra_names;
			while (param_i < args->descriptor->num_params) {
				bool found = false;
				while (src_param_i < src->descriptor->num_params) {
					if (args->descriptor->param_names[param_i] == src->descriptor->param_names[src_param_i]) {
						args->data[param_i] = src->data[src_param_i];
						found = true;
						++src_param_i;
						break;
					} else {
						extra_named_args[num_extra_named_args].name = src->descriptor->param_names[src_param_i];
						extra_named_args[num_extra_named_args].value = src->data[src_param_i];
						++num_extra_named_args;
						++src_param_i;
					}
				}
				
				if (!found) {
					// no name matched, so pick an unnamed arg, if AND ONLY IF the
					// target args don't already have something at that spot! This is
					// an important difference to snow_create_function_call_context.
					if (args->data[param_i] == NULL && src_unnamed_i < src->size) {
						args->data[param_i] = src->data[src_unnamed_i++];
					}
				}
				
				param_i++;
			}
			
			// Add remaining described named args as extra args
			while (src_param_i < src->descriptor->num_params) {
				extra_named_args[num_extra_named_args].name = src->descriptor->param_names[src_param_i];
				extra_named_args[num_extra_named_args].value = src->data[src_param_i];
				++num_extra_named_args;
				++src_param_i;
			}
			
			// Second, check the target arguments' describe named args against the source's
			// extra named args.
						fprintf(stderr, "Merge SnArguments, step 2\n");
			param_i = 0;
			size_t src_extra_i = 0;
			while (param_i < args->descriptor->num_params) {
				bool found = false;
				while (src_extra_i < src->num_extra_names) {
					const size_t src_extra_data_i = src->descriptor->num_params + src_extra_i;
					if (args->descriptor->param_names[param_i] == src->extra_names[src_extra_i]) {
						args->data[param_i] = src->data[src_extra_i];
						found = true;
						++src_extra_i;
						break;
					} else {
						extra_named_args[num_extra_named_args].name = src->extra_names[src_extra_i];
						extra_named_args[num_extra_named_args].value = src->data[src_extra_data_i];
						num_extra_named_args++;
						++src_extra_i;
					}
				}
				
				if (!found) {
					// no name matched, so pick an unnamed arg, as above.
					if (args->data[param_i] == NULL && src_unnamed_i < src->size) {
						args->data[param_i] = src->data[src_unnamed_i++];
					}
				}
			}
			
			// Add remaining extra named args as extra args
			while (src_extra_i < src->num_extra_names) {
				const size_t src_extra_data_i = src->descriptor->num_params + src_extra_i;
				extra_named_args[num_extra_named_args].name = src->extra_names[src_extra_i];
				extra_named_args[num_extra_named_args].value = src->data[src_extra_data_i];
				++num_extra_named_args;
				++src_extra_i;
			}
			
			// Third, put remaining unnamed args in extra_unnamed_args
						fprintf(stderr, "Merge SnArguments, step 3\n");
			for (size_t i = args->descriptor->num_params + args->num_extra_names; i < args->size; ++i) {
				extra_unnamed_args[num_extra_unnamed_args++] = args->data[i];
			}
			while (src_unnamed_i < src->size) {
				extra_unnamed_args[num_extra_unnamed_args++] = src->data[src_unnamed_i++];
			}
			
			// Fourth, do the merging!
			// The target described args are in good shape, we just need to
			// properly sort the extra named args, and append the unnamed args.
						fprintf(stderr, "Merge SnArguments, step 4\n");
			const size_t grow_by_names = num_extra_named_args;
			const size_t grow_by_args = grow_by_names + (src->size - src_unnamed_i);
			// Add existing extra named args to the pool, so they can be sorted together
			for (size_t i = 0; i < args->num_extra_names; ++i) {
				const size_t args_extra_data_i = args->descriptor->num_params + i;
				extra_named_args[num_extra_named_args].name = args->extra_names[i];
				extra_named_args[num_extra_named_args].value = args->data[args_extra_data_i];
				++num_extra_named_args;
			}
			
						fprintf(stderr, "Merge SnArguments, step 5\n");
			qsort(extra_named_args, num_extra_named_args, sizeof(struct named_arg), named_arg_cmp);
			
						fprintf(stderr, "Merge SnArguments, step 6\n");
			snow_arguments_grow_by(args, grow_by_names, grow_by_args);
			for (size_t i = 0; i < num_extra_named_args; ++i) {
				const size_t args_extra_data_i = args->descriptor->num_params + i;
				args->extra_names[i] = extra_named_args[i].name;
				args->data[args_extra_data_i] = extra_named_args[i].value;
			}
			for (size_t i = 0; i < num_extra_unnamed_args; ++i) {
				const size_t args_extra_data_i = args->descriptor->num_params + args->num_extra_names;
				args->data[args_extra_data_i] = extra_unnamed_args[i];
			}
			
			break;
		}
		default: {
			fprintf(stderr, "WARNING: Splat argument '%p' is not a map or array.\n", merge_in);
			break;
		}
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
	return snow_string_format("[FunctionCallContext@%p function:%p(%s)]", context, context->function, snow_sym_to_cstr(context->function->descriptor->name));
}

static VALUE function_call_context_get_arguments(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnFunctionCallContextType);
	SnFunctionCallContext* context = (SnFunctionCallContext*)self;
	return context->arguments;
}

SnObject* snow_create_function_call_context_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", function_call_context_inspect, 0);
	SN_DEFINE_PROPERTY(proto, "arguments", function_call_context_get_arguments, NULL);
	return proto;
}
