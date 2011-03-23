#include "globals.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/map.h"
#include "snow/module.h"
#include "snow/boolean.h"
#include "snow/vm.h"
#include "snow/numeric.h"
#include "snow/fiber.h"
#include "snow/exception.h"

static VALUE get_load_paths(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_get_load_paths();
}

SnObject* snow_get_vm_interface() {
	static SnObject* Snow = NULL;
	if (!Snow) {
		Snow = snow_create_object(NULL);
		snow_object_set_member(Snow, Snow, snow_sym("version"), snow_create_string_constant(snow_version()));
		SN_DEFINE_PROPERTY(Snow, "load_paths", get_load_paths, NULL);
	}
	return Snow;
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

static VALUE global_make_object(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	//ASSERT(it == SN_NIL || it == NULL || snow_type_of(it) == SnObjectType);
	SnObject* obj = snow_create_object(snow_eval_truth(it) ? (SnObject*)it : NULL);
	return obj;
}

static VALUE global_make_array(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_array_from_range(here->arguments->data, here->arguments->data + here->arguments->size);
}

static VALUE global_make_map(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_map();
}

static VALUE global_make_map_with_immediate_keys(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_map_with_immediate_keys();
}

static VALUE global_make_map_with_insertion_order(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_map_with_insertion_order();
}

static VALUE global_make_map_with_immediate_keys_and_insertion_order(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_map_with_immediate_keys_and_insertion_order();
}

static VALUE global_make_string(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return SN_NIL; // XXX: TODO!
}

static VALUE global_make_boolean(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_boolean_to_value(snow_eval_truth(it));
}

static VALUE global_make_symbol(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return SN_NIL; // XXX: TODO!
}

static VALUE global_make_fiber(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_fiber(it);
}

static VALUE global_disasm(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	SnType type = snow_type_of(it);
	if (type == SnFunctionType) {
		snow_vm_disassemble_function((SnFunction*)it);
	} else if (type == SnStringType) {
		snow_vm_disassemble_runtime_function(snow_string_cstr((SnString*)it));
	} else {
		snow_throw_exception_with_description("Cannot disassemble %s (not a function or name).", snow_value_inspect_cstr(it));
	}
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
	
	SN_DEFINE_GLOBAL("@", global_make_array, -1);
	SN_DEFINE_GLOBAL("#", global_make_map, -1);
	SN_DEFINE_GLOBAL("puts", global_puts, -1);
	SN_DEFINE_GLOBAL("import", global_import, 1);
	SN_DEFINE_GLOBAL("load", global_load, 1);
	SN_DEFINE_GLOBAL("__make_object__", global_make_object, -1);
	SN_DEFINE_GLOBAL("__make_array__", global_make_array, -1);
	SN_DEFINE_GLOBAL("__make_map__", global_make_map, -1);
	SN_DEFINE_GLOBAL("__make_map_with_immediate_keys__", global_make_map_with_immediate_keys, -1);
	SN_DEFINE_GLOBAL("__make_map_with_insertion_order__", global_make_map_with_insertion_order, -1);
	SN_DEFINE_GLOBAL("__make_map_with_immediate_keys_and_insertion_order__", global_make_map_with_immediate_keys_and_insertion_order, -1);
	SN_DEFINE_GLOBAL("__make_string__", global_make_string, -1);
	SN_DEFINE_GLOBAL("__make_boolean__", global_make_boolean, 1);
	SN_DEFINE_GLOBAL("__make_symbol__", global_make_symbol, 1);
	SN_DEFINE_GLOBAL("__make_fiber__", global_make_fiber, 1);
	SN_DEFINE_GLOBAL("__disasm__", global_disasm, 1);
	SN_DEFINE_GLOBAL("__resolve_symbol__", global_resolve_symbol, 1);
	SN_DEFINE_GLOBAL("__print_call_stack__", global_print_call_stack, 0);
	SN_DEFINE_GLOBAL("throw", global_throw, 1);
	
	snow_set_global(snow_sym("__integer_prototype__"), snow_get_prototype_for_type(SnIntegerType));
	snow_set_global(snow_sym("__nil_prototype__"), snow_get_prototype_for_type(SnNilType));
	snow_set_global(snow_sym("__boolean_prototype__"), snow_get_prototype_for_type(SnTrueType));
	snow_set_global(snow_sym("__symbol_prototype__"), snow_get_prototype_for_type(SnSymbolType));
	snow_set_global(snow_sym("__float_prototype__"), snow_get_prototype_for_type(SnFloatType));
	snow_set_global(snow_sym("__object_prototype__"), snow_get_prototype_for_type(SnObjectType));
	snow_set_global(snow_sym("__string_prototype__"), snow_get_prototype_for_type(SnStringType));
	snow_set_global(snow_sym("__array_prototype__"), snow_get_prototype_for_type(SnArrayType));
	snow_set_global(snow_sym("__map_prototype__"), snow_get_prototype_for_type(SnMapType));
	snow_set_global(snow_sym("__function_prototype__"), snow_get_prototype_for_type(SnFunctionType));
	snow_set_global(snow_sym("__call_frame_prototype__"), snow_get_prototype_for_type(SnCallFrameType));
	snow_set_global(snow_sym("__pointer_prototype__"), snow_get_prototype_for_type(SnPointerType));
	snow_set_global(snow_sym("__fiber_prototype__"), snow_get_prototype_for_type(SnFiberType));
}