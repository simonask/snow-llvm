#include "globals.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/type.h"

static VALUE global_puts(SnFunctionCallContext* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->arguments->size; ++i) {
		SnString* str = (SnString*)snow_call(snow_get_method(here->arguments->data[i], snow_sym("to_string")), here->arguments->data[i], 0, NULL);
		ASSERT(snow_type_of(str) == SnStringType);
		printf("%s", snow_string_cstr(str));
	}
	printf("\n");
	return SN_NIL;
}

void snow_init_globals() {
	snow_set_global(snow_sym("puts"), snow_create_method(global_puts, -1));
}