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
	
	snow_load_in_global_module(snow_create_string_constant("lib/prelude.sn"));
}

void snow_finish() {
	
}

const char* snow_version() {
	return "0.0.1 pre-alpha [x86-64]";
}

VALUE snow_get_global(SnSymbol name) {
	SnObject* go = snow_get_global_module();
	return snow_object_get_instance_variable(go, name);
}

VALUE snow_set_global(SnSymbol name, VALUE val) {
	SnObject* go = snow_get_global_module();
	return snow_object_set_instance_variable(go, name, val);
}

VALUE snow_local_missing(SnCallFrame* frame, SnSymbol name) {
	// XXX: TODO!!
	return snow_get_global(name);
	//fprintf(stderr, "LOCAL MISSING: %s\n", snow_sym_to_cstr(name));
	//return NULL;
}

SnObject* snow_get_immediate_class(SnValueType type) {
	ASSERT(type != SnPointerType);
	switch (type) {
		case SnIntegerType: return snow_get_integer_class();
		case SnNilType: return snow_get_nil_class();
		case SnFalseType:
		case SnTrueType: return snow_get_boolean_class();
		case SnSymbolType: return snow_get_symbol_class();
		case SnFloatType: return snow_get_float_class();
		default: TRAP(); return NULL;
	}
}

SnObject* snow_get_class(VALUE value) {
	if (snow_is_object(value)) {
		SnObject* object = (SnObject*)value;
		return object->cls ? object->cls : snow_get_object_class();
	} else {
		return snow_get_immediate_class(snow_type_of(value));
	}
}

VALUE snow_call(VALUE functor, VALUE self, size_t num_args, VALUE* args) {
	SnObject* function = snow_value_to_function(functor, &self);
	SnCallFrame frame = {
		.function = function,
		.caller = NULL, // set when call frame is pushed onto call chain stack
		.self = self,
		.locals = NULL, // set by snow_function_call,
		.args = {
			.num_names = 0,
			.names = NULL,
			.size = num_args,
			.data = (VALUE*)args,
		},
		.as_object = NULL,
	};
	return snow_function_call(function, &frame);
}

VALUE snow_call_with_arguments(VALUE functor, VALUE self, const SnArguments* args) {
	SnObject* function = snow_value_to_function(functor, &self);
	SnCallFrame frame = {
		.function = function,
		.caller = NULL,
		.self = self,
		.locals = NULL,
		.as_object = NULL,
	};
	memcpy(&frame.args, args, sizeof(SnArguments));
	return snow_function_call(function, &frame);
}

VALUE snow_call_method(VALUE self, SnSymbol method_name, size_t num_args, VALUE* args) {
	SnObject* cls = snow_get_class(self);
	struct SnMethod method;
	snow_class_get_method(cls, method_name, &method);
	VALUE func = NULL;
	if (method.type == SnMethodTypeFunction) {
		func = method.function;
	} else if (method.type == SnMethodTypeProperty) {
		func = snow_call(method.property->getter, self, 0, NULL);
	}
	if (func) {
		return snow_call(func, self, num_args, args);
	}
	return NULL;
}

VALUE snow_call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
	ASSERT(num_names <= num_args);
	SnObject* function = snow_value_to_function(functor, &self);
	SnCallFrame frame = {
		.function = function,
		.caller = NULL,
		.self = self,
		.locals = NULL,
		.args = {
			.num_names = num_names,
			.names = names,
			.size = num_args,
			.data = args,
		},
		.as_object = NULL,
	};
	return snow_function_call(function, &frame);
}

VALUE snow_value_freeze(VALUE it) {
	// TODO!!
	return it;
}

VALUE snow_get_module_value(SnObject* module, SnSymbol member) {
	VALUE v = snow_object_get_instance_variable(module, member);
	if (v) return v;
	snow_throw_exception_with_description("Variable '%s' not found in module %p.", snow_sym_to_cstr(member), module);
	return NULL;
}


SnObject* snow_value_to_string(VALUE it) {
	VALUE vstr = SNOW_CALL_METHOD(it, "to_string", 0, NULL);
	ASSERT(snow_is_string(vstr));
	return (SnObject*)vstr;
}

SnObject* snow_value_inspect(VALUE it) {
	VALUE vstr = SNOW_CALL_METHOD(it, "inspect", 0, NULL);
	ASSERT(snow_is_string(vstr));
	return (SnObject*)vstr;
}