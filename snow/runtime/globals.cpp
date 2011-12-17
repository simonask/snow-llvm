#include "globals.h"
#include "internal.h"
#include "snow/array.hpp"
#include "snow/boolean.h"
#include "snow/class.hpp"
#include "snow/exception.h"
#include "snow/fiber.hpp"
#include "snow/function.hpp"
#include "snow/map.h"
#include "snow/module.hpp"
#include "snow/nil.h"
#include "snow/numeric.h"
#include "snow/snow.h"
#include "snow/str.hpp"
#include "snow/value.h"
#include "snow/vm.h"


static VALUE get_load_paths(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow_get_load_paths();
}

static VALUE get_version(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_string_constant(snow_version());
}

CAPI SnObject* snow_get_vm_interface() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class(snow_sym("SnowVMInterface"), NULL);
		snow_class_define_property(cls, "version", get_version, NULL);
		SnObject* obj = snow_create_object(cls, 0, NULL);
		root = snow_gc_create_root(obj);
	}
	return *root;
}

static VALUE global_puts(const SnCallFrame* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->args->size; ++i) {
		SnObject* str = snow_value_to_string(here->args->data[i]);
		size_t sz = snow_string_size(str);
		char* buffer = (char*)alloca(sz+1);
		snow_string_copy_to(str, buffer, sz);
		buffer[sz] = '\0';
		fputs(buffer, stdout);
	}
	printf("\n");
	return SN_NIL;
}

static VALUE global_import(const SnCallFrame* here, VALUE self, VALUE it) {
	SnObject* file = snow_value_to_string(it);
	return snow_import(file);
}

static VALUE global_import_global(const SnCallFrame* here, VALUE self, VALUE it) {
	SnObject* file = snow_value_to_string(it);
	snow_load_in_global_module(file);
	return SN_TRUE;
}

static VALUE global_load(const SnCallFrame* here, VALUE self, VALUE it) {
	SnObject* file = snow_value_to_string(it);
	return snow_load(file);
}

static VALUE global_resolve_symbol(const SnCallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(it)) return NULL;
	int64_t n = snow_value_to_integer(it);
	const char* str = snow_sym_to_cstr(n);
	return str ? snow_create_string_constant(str) : NULL;
}

static VALUE global_print_call_stack(const SnCallFrame* here, VALUE self, VALUE it) {
	SnObject* fiber = snow_get_current_fiber();
	int level = 0;
	while (fiber) {
		SnCallFrame* frame = snow_fiber_get_current_frame(fiber);
		while (frame) {
			SnSymbol function_name = snow_function_get_name(frame->function);
			printf("%d: %s(", level++, snow_sym_to_cstr(function_name));
			// TODO: Print arguments
			printf(")\n");
			frame = frame->caller;
		}
		fiber = snow_fiber_get_link(fiber);
	}
	return SN_NIL;
}

static VALUE global_throw(const SnCallFrame* here, VALUE self, VALUE it) {
	snow_throw_exception(it);
	return NULL; // unreachable
}

#define SN_DEFINE_GLOBAL(NAME, FUNCTION, NUM_ARGS) snow_set_global(snow_sym(NAME), snow_create_function(FUNCTION, snow_sym(NAME)))

void snow_init_globals() {
	snow_set_global(snow_sym("Snow"), snow_get_vm_interface());
	
	SN_DEFINE_GLOBAL("puts", global_puts, -1);
	SN_DEFINE_GLOBAL("import", global_import, 1);
	SN_DEFINE_GLOBAL("import_global", global_import_global, 1);
	SN_DEFINE_GLOBAL("load", global_load, 1);
	SN_DEFINE_GLOBAL("__resolve_symbol__", global_resolve_symbol, 1);
	SN_DEFINE_GLOBAL("__print_call_stack__", global_print_call_stack, 0);
	SN_DEFINE_GLOBAL("throw", global_throw, 1);
	
	snow_set_global(snow_sym("Integer"), snow_get_integer_class());
	snow_set_global(snow_sym("Nil"), snow_get_nil_class());
	snow_set_global(snow_sym("Boolean"), snow_get_boolean_class());
	snow_set_global(snow_sym("Symbol"), snow_get_symbol_class());
	snow_set_global(snow_sym("Float"), snow_get_float_class());
	snow_set_global(snow_sym("Numeric"), snow_get_numeric_class());
	snow_set_global(snow_sym("Object"), snow_get_object_class());
	snow_set_global(snow_sym("Class"), snow_get_class_class());
	snow_set_global(snow_sym("String"), snow_get_string_class());
	snow_set_global(snow_sym("Array"), snow_get_array_class());
	snow_set_global(snow_sym("@"), snow_get_array_class());
	snow_set_global(snow_sym("Map"), snow_get_map_class());
	snow_set_global(snow_sym("#"), snow_get_map_class());
	snow_set_global(snow_sym("Function"), snow_get_function_class());
	snow_set_global(snow_sym("CallFrame"), snow_get_call_frame_class());
	snow_set_global(snow_sym("Fiber"), snow_get_fiber_class());
}