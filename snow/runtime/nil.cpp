#include "snow/nil.h"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/str.hpp"
#include "snow/function.hpp"

using namespace snow;

static VALUE nil_inspect(const CallFrame* here, VALUE self, VALUE it) {
	return snow::create_string_constant("nil");
}

static VALUE nil_to_string(const CallFrame* here, VALUE self, VALUE it) {
	return snow::create_string_constant("");
}

CAPI SnObject* snow_get_nil_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = create_class(snow_sym("Nil"), NULL);
		SN_DEFINE_METHOD(cls, "inspect", nil_inspect);
		SN_DEFINE_METHOD(cls, "to_string", nil_to_string);
		root = snow_gc_create_root(cls);
	}
	return *root;
}
