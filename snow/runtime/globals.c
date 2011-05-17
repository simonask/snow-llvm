#include "globals.h"
#include "internal.h"
#include "snow/array.h"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/fiber.h"
#include "snow/function.h"
#include "snow/map.h"
#include "snow/module.h"
#include "snow/nil.h"
#include "snow/numeric.h"
#include "snow/pointer.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/value.h"
#include "snow/vm.h"


static VALUE get_load_paths(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_get_load_paths();
}

static VALUE get_version(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_string_constant(snow_version());
}

SnObject* snow_get_vm_interface() {
	static VALUE* root = NULL;
	if (!root) {
		SnClass* cls = snow_create_class(snow_sym("SnowVMInterface"), NULL);
		snow_class_define_property(cls, "version", get_version, NULL);
		SnObject* obj = snow_create_object(cls);
		root = snow_gc_create_root(obj);
	}
	return (SnObject*)*root;
}

static VALUE global_puts(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->arguments->size; ++i) {
		SnString* str = snow_value_to_string(here->arguments->data[i]);
		printf("%s", snow_string_cstr(str));
	}
	printf("\n");
	return SN_NIL;
}

static VALUE global_import(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	SnString* file = snow_value_to_string(it);
	return snow_import(snow_string_cstr(file));
}

static VALUE global_load(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	SnString* file = snow_value_to_string(it);
	return snow_load(snow_string_cstr(file));
}

static VALUE global_disasm(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	SnType type = snow_type_of(it);
	snow_throw_exception_with_description("Disassembly not supported in native x86-64.", snow_value_inspect_cstr(it));
	return NULL;
}

static VALUE global_resolve_symbol(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(it)) return NULL;
	int64_t n = snow_value_to_integer(it);
	const char* str = snow_sym_to_cstr(n);
	return str ? snow_create_string_constant(str) : NULL;
}

static VALUE global_print_call_stack(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	SnFiber* fiber = snow_get_current_fiber();
	int level = 0;
	while (fiber) {
		SnCallFrame* frame = snow_fiber_get_current_frame(fiber);
		while (frame) {
			printf("%d: %s(", level++, snow_sym_to_cstr(frame->function->descriptor->name));
			if (frame->arguments) {
				size_t num_args = frame->arguments->size;
				size_t arg_i = 0;
				for (size_t i = 0; i < frame->function->descriptor->num_params && arg_i < num_args; ++i) {
					printf("%s: %p", snow_sym_to_cstr(frame->function->descriptor->param_names[i]), frame->arguments->data[arg_i++]);
					if (arg_i < num_args-1) printf(", ");
				}
				for (size_t i = 0; i < frame->arguments->num_extra_names && arg_i < num_args; ++i) {
					printf("%s: %p, ", snow_sym_to_cstr(frame->arguments->extra_names[i]), frame->arguments->data[arg_i++]);
					if (arg_i < num_args-1) printf(", ");
				}
				for (size_t i = arg_i; i < num_args; ++i) {
					printf("%p", frame->arguments->data[arg_i++]);
					if (i < num_args-1) printf(", ");
				}
			}
			printf(")\n");
			frame = frame->caller;
		}
		fiber = fiber->link;
	}
	return SN_NIL;
}

static VALUE global_throw(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	snow_throw_exception(it);
	return NULL;
}

#define SN_DEFINE_GLOBAL(NAME, FUNCTION, NUM_ARGS) snow_set_global(snow_sym(NAME), snow_create_method(FUNCTION, snow_sym(NAME), NUM_ARGS))

void snow_init_globals() {
	snow_set_global(snow_sym("Snow"), snow_get_vm_interface());
	
	SN_DEFINE_GLOBAL("puts", global_puts, -1);
	SN_DEFINE_GLOBAL("import", global_import, 1);
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
	snow_set_global(snow_sym("Pointer"), snow_get_pointer_class());
	snow_set_global(snow_sym("Fiber"), snow_get_fiber_class());
}