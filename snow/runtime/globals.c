#include "globals.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/array.h"

static VALUE global_puts(SnFunctionCallContext* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->arguments->size; ++i) {
		SnString* str = (SnString*)snow_call(snow_get_method(here->arguments->data[i], snow_sym("to_string")), here->arguments->data[i], 0, NULL);
		ASSERT(snow_type_of(str) == SnStringType);
		printf("%s", snow_string_cstr(str));
	}
	printf("\n");
	return SN_NIL;
}

static VALUE global_make_array(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_array_from_range(here->arguments->data, here->arguments->data + here->arguments->size);
}

void snow_init_globals() {
	snow_set_global(snow_sym("@"), snow_create_method(global_make_array, -1));
	snow_set_global(snow_sym("puts"), snow_create_method(global_puts, -1));
	
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
	//snow_set_global(snow_sym("__pointer_prototype__"), snow_get_prototype_for_type(SnPointerType));
}