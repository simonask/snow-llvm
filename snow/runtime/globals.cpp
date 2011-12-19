#include "globals.h"
#include "internal.h"
#include "snow/array.hpp"
#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/exception.h"
#include "snow/fiber.hpp"
#include "snow/function.hpp"
#include "snow/map.hpp"
#include "snow/module.hpp"
#include "snow/nil.h"
#include "snow/numeric.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/value.hpp"
#include "snow/vm.h"

using namespace snow;


static VALUE get_load_paths(const CallFrame* here, VALUE self, VALUE it) {
	return get_load_paths();
}

static VALUE get_version(const CallFrame* here, VALUE self, VALUE it) {
	return create_string_constant(snow::version());
}

CAPI SnObject* snow_get_vm_interface() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = create_class(snow_sym("SnowVMInterface"), NULL);
		SN_DEFINE_PROPERTY(cls, "version", get_version, NULL);
		SnObject* obj = snow_create_object(cls, 0, NULL);
		root = snow_gc_create_root(obj);
	}
	return *root;
}

static VALUE global_puts(const CallFrame* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->args->size; ++i) {
		SnObject* str = value_to_string(here->args->data[i]);
		size_t sz = string_size(str);
		char* buffer = (char*)alloca(sz+1);
		string_copy_to(str, buffer, sz);
		buffer[sz] = '\0';
		fputs(buffer, stdout);
	}
	printf("\n");
	return SN_NIL;
}

static VALUE global_import(const CallFrame* here, VALUE self, VALUE it) {
	SnObject* file = value_to_string(it);
	return import(file);
}

static VALUE global_import_global(const CallFrame* here, VALUE self, VALUE it) {
	SnObject* file = value_to_string(it);
	load_in_global_module(file);
	return SN_TRUE;
}

static VALUE global_load(const CallFrame* here, VALUE self, VALUE it) {
	SnObject* file = value_to_string(it);
	return load(file);
}

static VALUE global_resolve_symbol(const CallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(it)) return NULL;
	int64_t n = snow_value_to_integer(it);
	const char* str = snow_sym_to_cstr(n);
	return str ? create_string_constant(str) : create_string_constant("<invalid symbol>");
}

static VALUE global_print_call_stack(const CallFrame* here, VALUE self, VALUE it) {
	SnObject* fiber = snow_get_current_fiber();
	int level = 0;
	while (fiber) {
		CallFrame* frame = snow_fiber_get_current_frame(fiber);
		while (frame) {
			SnSymbol function_name = function_get_name(frame->function);
			printf("%d: %s(", level++, snow_sym_to_cstr(function_name));
			// TODO: Print arguments
			printf(")\n");
			frame = frame->caller;
		}
		fiber = snow_fiber_get_link(fiber);
	}
	return SN_NIL;
}

static VALUE global_throw(const CallFrame* here, VALUE self, VALUE it) {
	snow_throw_exception(it);
	return NULL; // unreachable
}

#define SN_DEFINE_GLOBAL(NAME, FUNCTION, NUM_ARGS) snow::set_global(snow_sym(NAME), snow::create_function(FUNCTION, snow_sym(NAME)))

void snow_init_globals() {
	set_global(snow_sym("Snow"), snow_get_vm_interface());
	
	SN_DEFINE_GLOBAL("puts", global_puts, -1);
	SN_DEFINE_GLOBAL("import", global_import, 1);
	SN_DEFINE_GLOBAL("import_global", global_import_global, 1);
	SN_DEFINE_GLOBAL("load", global_load, 1);
	SN_DEFINE_GLOBAL("__resolve_symbol__", global_resolve_symbol, 1);
	SN_DEFINE_GLOBAL("__print_call_stack__", global_print_call_stack, 0);
	SN_DEFINE_GLOBAL("throw", global_throw, 1);
	
	set_global(snow_sym("Integer"), snow_get_integer_class());
	set_global(snow_sym("Nil"), snow_get_nil_class());
	set_global(snow_sym("Boolean"), snow_get_boolean_class());
	set_global(snow_sym("Symbol"), snow_get_symbol_class());
	set_global(snow_sym("Float"), snow_get_float_class());
	set_global(snow_sym("Numeric"), snow_get_numeric_class());
	set_global(snow_sym("Object"), snow_get_object_class());
	set_global(snow_sym("Class"), get_class_class());
	set_global(snow_sym("String"), get_string_class());
	set_global(snow_sym("Array"), get_array_class());
	set_global(snow_sym("@"), get_array_class());
	set_global(snow_sym("Map"), snow_get_map_class());
	set_global(snow_sym("#"), snow_get_map_class());
	set_global(snow_sym("Function"), get_function_class());
	set_global(snow_sym("Environment"), get_environment_class());
	set_global(snow_sym("Fiber"), snow_get_fiber_class());
}