#include "globals.h"
#include "internal.h"
#include "snow/array.hpp"
#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/exception.hpp"
#include "snow/fiber.hpp"
#include "snow/function.hpp"
#include "snow/map.hpp"
#include "snow/module.hpp"
#include "snow/nil.hpp"
#include "snow/numeric.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/value.hpp"

using namespace snow;


static Value get_load_paths(const CallFrame* here, const Value& self, const Value& it) {
	return get_load_paths();
}

static Value get_version(const CallFrame* here, const Value& self, const Value& it) {
	return create_string_constant(snow::version());
}

Value snow_get_vm_interface() {
	static Value* root = NULL;
	if (!root) {
		ObjectPtr<Class> cls = create_class(snow::sym("SnowVMInterface"), NULL);
		SN_DEFINE_PROPERTY(cls, "version", get_version, NULL);
		Value obj = create_object(cls, 0, NULL);
		root = gc_create_root(obj);
	}
	return *root;
}

static Value global_puts(const CallFrame* here, const Value& self, const Value& it) {
	for (size_t i = 0; i < here->args->size; ++i) {
		ObjectPtr<String> str = value_to_string(here->args->data[i]);
		size_t sz = string_size(str);
		char* buffer = (char*)alloca(sz+1);
		string_copy_to(str, buffer, sz);
		buffer[sz] = '\0';
		fputs(buffer, stdout);
	}
	printf("\n");
	return SN_NIL;
}

static Value global_import(const CallFrame* here, const Value& self, const Value& it) {
	ObjectPtr<String> file = value_to_string(it);
	return import(file);
}

static Value global_import_global(const CallFrame* here, const Value& self, const Value& it) {
	ObjectPtr<String> file = value_to_string(it);
	load_in_global_module(file);
	return SN_TRUE;
}

static Value global_load(const CallFrame* here, const Value& self, const Value& it) {
	ObjectPtr<String> file = value_to_string(it);
	return load(file);
}

static Value global_resolve_symbol(const CallFrame* here, const Value& self, const Value& it) {
	if (!is_integer(it)) return NULL;
	int64_t n = value_to_integer(it);
	const char* str = snow::sym_to_cstr(n);
	return str ? create_string_constant(str) : create_string_constant("<invalid symbol>");
}

static Value global_print_call_stack(const CallFrame* here, const Value& self, const Value& it) {
	ObjectPtr<Fiber> fiber = get_current_fiber();
	int level = 0;
	while (fiber) {
		CallFrame* frame = fiber_get_current_frame(fiber);
		while (frame) {
			Symbol function_name = function_get_name(frame->function);
			printf("%d: %s(", level++, snow::sym_to_cstr(function_name));
			// TODO: Print arguments
			printf(")\n");
			frame = frame->caller;
		}
		fiber = fiber_get_link(fiber);
	}
	return SN_NIL;
}

static Value global_throw(const CallFrame* here, const Value& self, const Value& it) {
	throw_exception(it);
	return NULL; // unreachable
}

#define SN_DEFINE_GLOBAL(NAME, FUNCTION, NUM_ARGS) snow::set_global(snow::sym(NAME), snow::create_function(FUNCTION, snow::sym(NAME)))

void snow_init_globals() {
	set_global(snow::sym("Snow"), snow_get_vm_interface());
	
	SN_DEFINE_GLOBAL("puts", global_puts, -1);
	SN_DEFINE_GLOBAL("import", global_import, 1);
	SN_DEFINE_GLOBAL("import_global", global_import_global, 1);
	SN_DEFINE_GLOBAL("load", global_load, 1);
	SN_DEFINE_GLOBAL("__resolve_symbol__", global_resolve_symbol, 1);
	SN_DEFINE_GLOBAL("__print_call_stack__", global_print_call_stack, 0);
	SN_DEFINE_GLOBAL("throw", global_throw, 1);
	
	set_global(snow::sym("Integer"), get_integer_class());
	set_global(snow::sym("Nil"), get_nil_class());
	set_global(snow::sym("Boolean"), get_boolean_class());
	set_global(snow::sym("Symbol"), get_symbol_class());
	set_global(snow::sym("Float"), get_float_class());
	set_global(snow::sym("Numeric"), get_numeric_class());
	set_global(snow::sym("Object"), get_object_class());
	set_global(snow::sym("Class"), get_class_class());
	set_global(snow::sym("String"), get_string_class());
	set_global(snow::sym("Array"), get_array_class());
	set_global(snow::sym("@"), get_array_class());
	set_global(snow::sym("Map"), get_map_class());
	set_global(snow::sym("#"), get_map_class());
	set_global(snow::sym("Function"), get_function_class());
	set_global(snow::sym("Environment"), get_environment_class());
	set_global(snow::sym("Fiber"), get_fiber_class());
}