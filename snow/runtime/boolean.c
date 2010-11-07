#include "snow/boolean.h"
#include "snow/object.h"
#include "snow/function.h"
#include "snow/str.h"

static VALUE boolean_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return snow_create_string_constant(snow_value_to_boolean(self) ? "true" : "false");
}

SnObject* snow_create_boolean_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", boolean_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", boolean_inspect, 0);
	return proto;
}