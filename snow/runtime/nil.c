#include "snow/nil.h"
#include "internal.h"
#include "snow/class.h"
#include "snow/type.h"
#include "snow/str.h"
#include "snow/function.h"

static VALUE nil_inspect(SnFunction* function, SnCallFrame* context, VALUE self, VALUE it) {
	return snow_create_string_constant("nil");
}

static VALUE nil_to_string(SnFunction* function, SnCallFrame* context, VALUE self, VALUE it) {
	return snow_create_string_constant("");
}

SnClass* snow_get_nil_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnClass* cls = snow_create_class(snow_sym("Nil"), NULL);
		snow_class_define_method(cls, "inspect", nil_inspect, 0);
		snow_class_define_method(cls, "to_string", nil_to_string, 0);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}
