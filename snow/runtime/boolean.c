#include "snow/boolean.h"
#include "internal.h"
#include "snow/class.h"
#include "snow/function.h"
#include "snow/str.h"

static VALUE boolean_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow_create_string_constant(snow_value_to_boolean(self) ? "true" : "false");
}

static VALUE boolean_complement(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow_boolean_to_value(!snow_value_to_boolean(self));
}

SnObject* snow_get_boolean_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class(snow_sym("Boolean"), NULL);
		snow_class_define_method(cls, "inspect", boolean_inspect);
		snow_class_define_method(cls, "to_string", boolean_inspect);
		snow_class_define_method(cls, "~", boolean_complement);	
		root = snow_gc_create_root(cls);
	}
	return *root;
}
