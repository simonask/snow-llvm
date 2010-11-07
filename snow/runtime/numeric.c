#include "snow/numeric.h"
#include "snow/value.h"
#include "snow/type.h"
#include "snow/object.h"
#include "snow/process.h"
#include "snow/function.h"
#include "snow/snow.h"
#include "snow/str.h"

static VALUE numeric_add(SnFunctionCallContext* here, VALUE self, VALUE it) {
	const bool is_self_int = snow_is_integer(self);
	const bool is_it_int = snow_is_integer(it);
	if (is_self_int && is_it_int) {
		return snow_integer_to_value(snow_value_to_integer(self) + snow_value_to_integer(it));
	} else {
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_float_to_value(a + b);
	}
}

static VALUE numeric_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_is_integer(self)) {
		char buffer[100];
		snprintf(buffer, 100, "%d", snow_value_to_integer(self));
		return snow_create_string(buffer);
	} else if (snow_is_float(self)) {
		char buffer[100];
		snprintf(buffer, 100, "%f", snow_value_to_float(self));
		return snow_create_string(buffer);
	}
	return SN_NIL;
}

SnObject* snow_create_integer_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "+", numeric_add, 1);
	SN_DEFINE_METHOD(proto, "inspect", numeric_inspect, 0);
	return proto;
}

SnObject* snow_create_float_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "+", numeric_add, 1);
	SN_DEFINE_METHOD(proto, "inspect", numeric_inspect, 0);
	return proto;
}
