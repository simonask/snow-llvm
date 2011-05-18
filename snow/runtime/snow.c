#include "snow/snow.h"
#include "internal.h"
#include "globals.h"
#include "snow/array.h"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/fiber.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/map.h"
#include "snow/module.h"
#include "snow/nil.h"
#include "snow/numeric.h"
#include "snow/object.h"
#include "snow/parser.h"
#include "snow/pointer.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/vm.h"
#include "snow/vm.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

void snow_lex(const char*);
void snow_init_fibers();

struct SnAstNode;

void snow_init(const char* lib_path) {
	const void* stk;
	snow_init_gc(&stk);
	snow_init_fibers();
	snow_init_globals();
	
	snow_load_in_global_module("lib/prelude.sn");
}

void snow_finish() {
	
}

const char* snow_version() {
	return "0.0.1 pre-alpha [x86-64]";
}

VALUE snow_get_global(SnSymbol name) {
	SnObject* go = snow_get_global_module();
	return snow_object_get_field(go, name);
}

VALUE snow_set_global(SnSymbol name, VALUE val) {
	SnObject* go = snow_get_global_module();
	return snow_object_set_field(go, name, val);
}

VALUE snow_local_missing(SnCallFrame* frame, SnSymbol name) {
	// XXX: TODO!!
	return snow_get_global(name);
	//fprintf(stderr, "LOCAL MISSING: %s\n", snow_sym_to_cstr(name));
	//return NULL;
}

static SnClass* class_for_internal_type(SnType type) {
	switch (type) {
		case SnIntegerType:   return snow_get_integer_class();
		case SnNilType:       return snow_get_nil_class();
		case SnFalseType:
		case SnTrueType:      return snow_get_boolean_class();
		case SnSymbolType:    return snow_get_symbol_class();
		case SnFloatType:     return snow_get_float_class();
		
		case SnObjectType:    return snow_get_object_class();
		case SnClassType:     return snow_get_class_class();
		case SnArrayType:     return snow_get_array_class();
		case SnMapType:       return snow_get_map_class();
		case SnStringType:    return snow_get_string_class();
		case SnFunctionType:  return snow_get_function_class();
		case SnCallFrameType: return snow_get_call_frame_class();
		case SnArgumentsType: return snow_get_arguments_class();
		case SnPointerType:   return snow_get_pointer_class();
		case SnFiberType:     return snow_get_fiber_class();
		default: {
			ASSERT(false && "Unknown type!");
		}
	}
	return NULL;
}

SnClass* snow_get_class(VALUE value) {
	if (snow_is_object(value)) {
		SnObject* object = (SnObject*)value;
		return object->cls ? object->cls : class_for_internal_type(object->type);
	}
	return class_for_internal_type(snow_type_of(value));
}

VALUE snow_call(VALUE functor, VALUE self, size_t num_args, const VALUE* args) {
	SnFunction* function = snow_value_to_function(functor, &self);
	SnCallFrame* context = snow_create_call_frame(function, 0, NULL, num_args, args);
	return snow_function_call(function, context, self, num_args ? args[0] : NULL);
}

VALUE snow_call_with_arguments(VALUE functor, VALUE self, SnArguments* args) {
	SnFunction* function = snow_value_to_function(functor, &self);
	SnCallFrame* context = snow_create_call_frame_with_arguments(function, args);
	return snow_function_call(function, context, self, args ? (args->size ? args->data[0] : NULL) : NULL);
}

VALUE snow_call_method(VALUE self, SnSymbol method_name, size_t num_args, const VALUE* args) {
	SnClass* cls = snow_get_class(self);
	SnMethod method;
	snow_class_get_method_or_property(cls, method_name, &method);
	VALUE func = NULL;
	if (method.type == SnMethodTypeFunction) {
		func = method.function;
	} else if (method.type == SnMethodTypeProperty) {
		func = snow_call(method.property.getter, self, 0, NULL);
	}
	if (func) {
		return snow_call(func, self, num_args, args);
	}
	return NULL;
}

struct named_arg { SnSymbol name; VALUE value; };

static int named_arg_cmp(const void* _a, const void* _b) {
	const struct named_arg* a = (struct named_arg*)_a;
	const struct named_arg* b = (struct named_arg*)_b;
	return (int)((int64_t)b->name - (int64_t)a->name);
}

VALUE snow_call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
	ASSERT(num_names <= num_args);
	VALUE it = num_args ? args[0] : NULL;
	if (num_names) {
		// TODO: Sort names in-place.
		struct named_arg* tuples = alloca(sizeof(struct named_arg)*num_names);
		for (size_t i = 0; i < num_names; ++i) { tuples[i].name = names[i]; tuples[i].value = args[i]; }
		qsort(tuples, num_names, sizeof(struct named_arg), named_arg_cmp);
		for (size_t i = 0; i < num_names; ++i) { names[i] = tuples[i].name; args[i] = tuples[i].value; }
		it = args[0];
	}
	
	SnFunction* function = snow_value_to_function(functor, &self);
	SnCallFrame* context = snow_create_call_frame(function, num_names, names, num_args, args);
	return snow_function_call(function, context, self, it);
}

VALUE snow_value_freeze(VALUE it) {
	// TODO!!
	return it;
}

VALUE snow_get_module_value(SnObject* module, SnSymbol member) {
	VALUE v = snow_object_get_field(module, member);
	if (v) return v;
	snow_throw_exception_with_description("Variable '%s' not found in module '%s'.", snow_sym_to_cstr(member), snow_value_inspect_cstr(module));
	return NULL;
}


SnString* snow_value_to_string(VALUE it) {
	VALUE vstr = SNOW_CALL_METHOD(it, "to_string", 0, NULL);
	ASSERT(snow_type_of(vstr) == SnStringType);
	return (SnString*)vstr;
}

SnString* snow_value_inspect(VALUE it) {
	VALUE vstr = SNOW_CALL_METHOD(it, "inspect", 0, NULL);
	ASSERT(snow_type_of(vstr) == SnStringType);
	return (SnString*)vstr;
}

const char* snow_value_to_cstr(VALUE it) {
	return snow_string_cstr(snow_value_to_string(it));
}

const char* snow_value_inspect_cstr(VALUE it) {
	return snow_string_cstr(snow_value_inspect(it));
}
