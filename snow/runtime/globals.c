#include "globals.h"
#include "snow/snow.h"
#include "snow/function.h"
#include "snow/str.h"

static VALUE global_puts(SnFunctionCallContext* here, VALUE self, VALUE it) {
	for (size_t i = 0; i < here->extra->num_args; ++i) {
		SnString* str = (SnString*)snow_call(snow_get_method(here->locals[i], snow_sym("to_string")), here->locals[i], 0, NULL);
		ASSERT(snow_type_of(str) == SnStringType);
		printf("%s", snow_string_cstr(str));
	}
	printf("\n");
	return SN_NIL;
}

void snow_init_globals() {
	snow_set_global(snow_sym("puts"), snow_create_method(global_puts, -1));
}