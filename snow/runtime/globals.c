#include "globals.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/module.h"

static VALUE get_load_paths(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_get_load_paths();
}

SnObject* snow_get_vm_interface() {
	static SnObject* Snow = NULL;
	if (!Snow) {
		Snow = snow_create_object(NULL);
		snow_object_set_member(Snow, Snow, snow_sym("version"), snow_create_string(snow_version()));
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

static VALUE global_make_array(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_array_from_range(here->arguments->data, here->arguments->data + here->arguments->size);
}

static VALUE global_make_object(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(it == SN_NIL || it == NULL || snow_type_of(it) == SnObjectType);
	return snow_create_object(snow_eval_truth(it) ? (SnObject*)it : NULL);
}

static VALUE global_import(SnFunctionCallContext* here, VALUE self, VALUE it) {
	SnString* file = snow_value_to_string(it);
	return snow_import(snow_string_cstr(file));
}

static VALUE global_load(SnFunctionCallContext* here, VALUE self, VALUE it) {
	SnString* file = snow_value_to_string(it);
	return snow_load(snow_string_cstr(file));
}

#define SN_DEFINE_GLOBAL(NAME, FUNCTION, NUM_ARGS) snow_set_global(snow_sym(NAME), snow_create_method(FUNCTION, snow_sym(NAME), NUM_ARGS))

void snow_init_globals() {
	snow_set_global(snow_sym("Snow"), snow_get_vm_interface());
	
	SN_DEFINE_GLOBAL("@", global_make_array, -1);
	SN_DEFINE_GLOBAL("puts", global_puts, -1);
	SN_DEFINE_GLOBAL("import", global_import, 1);
	SN_DEFINE_GLOBAL("load", global_load, 1);
	SN_DEFINE_GLOBAL("__make_object__", global_make_object, -1);
	
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
}