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
		SnMethod methods[] = {
			SN_METHOD("inspect", nil_inspect, 0),
			SN_METHOD("to_string", nil_to_string, 0),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Nil"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}
