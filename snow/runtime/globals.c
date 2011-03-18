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
#include "snow/continuation.h"

static VALUE get_load_paths(SnFunctionCallContext* here, VALUE self, VALUE it) {
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

static VALUE global_puts(SnFunctionCallContext* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->arguments->size; ++i) {
		SnString* str = snow_value_to_string(here->arguments->data[i]);
		printf("%s", snow_string_cstr(str));
	}
	printf("\n");
	return SN_NIL;
}

static VALUE global_import(SnFunctionCallContext* here, VALUE self, VALUE it) {
	SnString* file = snow_value_to_string(it);
	return snow_import(snow_string_cstr(file));
}

static VALUE global_load(SnFunctionCallContext* here, VALUE self, VALUE it) {
	SnString* file = snow_value_to_string(it);
	return snow_load(snow_string_cstr(file));
}

static VALUE global_make_object(SnFunctionCallContext* here, VALUE self, VALUE it) {
	//ASSERT(it == SN_NIL || it == NULL || snow_type_of(it) == SnObjectType);
	SnObject* obj = snow_create_object(snow_eval_truth(it) ? (SnObject*)it : NULL);
	return obj;
}

static VALUE global_make_array(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_array_from_range(here->arguments->data, here->arguments->data + here->arguments->size);
}

static VALUE global_make_map(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_map();
}

static VALUE global_make_map_with_immediate_keys(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_map_with_immediate_keys();
}

static VALUE global_make_map_with_insertion_order(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_map_with_insertion_order();
}

static VALUE global_make_map_with_immediate_keys_and_insertion_order(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_map_with_immediate_keys_and_insertion_order();
}

static VALUE global_make_string(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return SN_NIL; // XXX: TODO!
}

static VALUE global_make_boolean(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_boolean_to_value(snow_eval_truth(it));
}

static VALUE global_make_symbol(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return SN_NIL; // XXX: TODO!
}

static VALUE global_make_continuation(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_continuation(it);
}

static VALUE global_disasm(SnFunctionCallContext* here, VALUE self, VALUE it) {
	VALUE dummy = NULL;
	SnFunction* function = snow_value_to_function(it, &dummy);
	if (function) {
		snow_vm_print_disassembly(function);
	} else {
		fprintf(stderr, "Cannot convert %p to a function.\n", it);
	}
	return NULL;
}

static VALUE global_resolve_symbol(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (!snow_is_integer(it)) return NULL;
	int64_t n = snow_value_to_integer(it);
	const char* str = snow_sym_to_cstr(n);
	return str ? snow_create_string_constant(str) : NULL;
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
	SN_DEFINE_GLOBAL("__make_continuation__", global_make_continuation, 1);
	SN_DEFINE_GLOBAL("__disasm__", global_disasm, 1);
	SN_DEFINE_GLOBAL("__resolve_symbol__", global_resolve_symbol, 1);
	
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
	snow_set_global(snow_sym("__function_call_context_prototype__"), snow_get_prototype_for_type(SnFunctionCallContextType));
	snow_set_global(snow_sym("__pointer_prototype__"), snow_get_prototype_for_type(SnPointerType));
	snow_set_global(snow_sym("__continuation_prototype__"), snow_get_prototype_for_type(SnContinuationType));
}