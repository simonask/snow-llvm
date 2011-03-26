#include "snow/nil.h"
#include "snow/type.h"
#include "snow/str.h"
#include "snow/function.h"

static VALUE nil_inspect(SnFunction* function, SnCallFrame* context, VALUE self, VALUE it) {
	return snow_create_string_constant("nil");
}

static VALUE nil_to_string(SnFunction* function, SnCallFrame* context, VALUE self, VALUE it) {
	return snow_create_string_constant("");
}

SnObject* snow_create_nil_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", nil_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", nil_to_string, 0);
	return proto;
}