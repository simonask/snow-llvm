#include "snow/snow.h"
#include "snow/vm.h"
#include "snow/process.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/numeric.h"
#include "snow/boolean.h"
#include "snow/nil.h"
#include "snow/str.h"
#include "snow/parser.h"
#include "snow/vm.h"
#include "snow/function.h"
#include "snow/map.h"
#include "snow/exception.h"
#include "snow/gc.h"
#include "snow/module.h"
#include "globals.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

void snow_lex(const char*);

struct SnAstNode;

static SnProcess main_process;
static SnObject* immediate_prototypes[8];

SnProcess* snow_init(struct SnVM* vm) {
	main_process.vm = vm;
	
	const void* stk;
	snow_init_gc(&stk);
	snow_init_globals();
	
	snow_load_in_global_module("lib/prelude.sn");
	
	return &main_process;
}

const char* snow_version() {
	return "0.0.1 pre-alpha [LLVM]";
}

SnProcess* snow_get_process() {
	return &main_process;
}

VALUE snow_get_global(SnSymbol name) {
	SnObject* go = snow_get_global_module();
	return snow_object_get_member(go, go, name);
}

VALUE snow_set_global(SnSymbol name, VALUE val) {
	SnObject* go = snow_get_global_module();
	return snow_object_set_member(go, go, name, val);
}

SnFunction* snow_compile(const char* module_name, const char* source) {
	struct SnAST* ast = snow_parse(source);
	if (ast) {
		SnCompilationResult result;
		if (snow_vm_compile_ast(module_name, source, ast, &result)) {
			return snow_create_function(result.entry_descriptor, NULL);
		} else if (result.error_str) {
			fprintf(stderr, "ERROR COMPILING FUNCTION: %s\n", result.error_str);
			free(result.error_str);
			return NULL;
		} else {
			fprintf(stderr, "ERROR COMPILING FUNCTION: <UNKNOWN>\n");
			return NULL;
		}
	}
	return NULL;
}

VALUE snow_call(VALUE functor, VALUE self, size_t num_args, const VALUE* args) {
	SnFunction* function = snow_value_to_function(functor, &self);
	SnFunctionCallContext* context = snow_create_function_call_context(function, NULL, 0, NULL, num_args, args);
	return snow_function_call(function, context, self, num_args ? args[0] : NULL);
}

VALUE snow_call_method(VALUE self, SnSymbol method_name, size_t num_args, const VALUE* args) {
	SnFunction* method = snow_get_method(self, method_name, &self);
	if (!method) return NULL;
	ASSERT(snow_type_of(method) == SnFunctionType);
	return snow_call(method, self, num_args, args);
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
	SnFunctionCallContext* context = snow_create_function_call_context(function, NULL, num_names, names, num_args, args);
	return snow_function_call(function, context, self, it);
}

VALUE snow_call_method_with_named_arguments(VALUE self, SnSymbol method_name, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
	SnFunction* method = snow_get_method(self, method_name, &self);
	return snow_call_with_named_arguments(method, self, num_names, names, num_args, args);
}

SnFunction* snow_get_method(VALUE object, SnSymbol member, VALUE* new_self) {
	// TODO: Support functor objects
	VALUE f = snow_get_member(object, member);
	*new_self = object;
	if (!f) {
		snow_throw_exception_with_description("Object %p does not respond to method '%s'.", object, snow_sym_to_cstr(member));
		return NULL;
	}
	return snow_value_to_function(f, new_self);
}

SnObject* snow_get_nearest_object(VALUE value) {
	SnType type = snow_type_of(value);
	if (type == SnObjectType) return (SnObject*)value;
	return snow_get_prototype_for_type(type);
}

VALUE snow_get_member(VALUE self, SnSymbol member) {
	SnObject* prototype = snow_get_nearest_object(self);
	return snow_object_get_member(prototype, self, member);
}

VALUE snow_set_member(VALUE self, SnSymbol member, VALUE val) {
	SnObject* prototype = snow_get_nearest_object(self);
	return snow_object_set_member(prototype, self, member, val);
}

VALUE snow_value_freeze(VALUE it) {
	// TODO!!
	return it;
}

VALUE snow_get_module_value(SnObject* module, SnSymbol member) {
	VALUE v = snow_get_member(module, member);
	if (v) return v;
	// TODO: Exception
	fprintf(stderr, "ERROR: Variable '%s' not found in module %p.\n", snow_sym_to_cstr(member), module);
	return NULL;
}

SnObject* snow_create_class_for_prototype(SnSymbol name, SnObject* proto) {
	SnObject* class_prototype = snow_get_global(snow_sym("__class_prototype__"));
	ASSERT(class_prototype); // prelude has not been loaded
	SnObject* klass = snow_create_object(class_prototype);
	klass->name = name;
	snow_object_set_member(klass, klass, snow_sym("instance_prototype"), proto);
	snow_object_set_member(proto, proto, snow_sym("class"), klass);
	return klass;
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
