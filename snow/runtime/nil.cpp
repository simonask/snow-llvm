#include "snow/nil.h"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/str.hpp"
#include "snow/function.hpp"

static VALUE nil_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow::create_string_constant("nil");
}

static VALUE nil_to_string(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow::create_string_constant("");
}

CAPI SnObject* snow_get_nil_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class(snow_sym("Nil"), NULL);
		snow_class_define_method(cls, "inspect", nil_inspect);
		snow_class_define_method(cls, "to_string", nil_to_string);
		root = snow_gc_create_root(cls);
	}
	return *root;
}
