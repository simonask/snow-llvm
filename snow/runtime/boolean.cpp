#include "snow/boolean.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/str.hpp"

using namespace snow;

static VALUE boolean_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow::create_string_constant(snow_value_to_boolean(self) ? "true" : "false");
}

static VALUE boolean_complement(const SnCallFrame* here, VALUE self, VALUE it) {
	return snow_boolean_to_value(!snow_value_to_boolean(self));
}

CAPI SnObject* snow_get_boolean_class() {
	static SnObject** root = NULL;
	if (!root) {
		ObjectPtr<Class> cls = create_class(snow_sym("Boolean"), NULL);
		SN_DEFINE_METHOD(cls, "inspect", boolean_inspect);
		SN_DEFINE_METHOD(cls, "to_string", boolean_inspect);
		SN_DEFINE_METHOD(cls, "~", boolean_complement);	
		root = snow_gc_create_root(cls);
	}
	return *root;
}
