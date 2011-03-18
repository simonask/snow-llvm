#include "snow/numeric.h"
#include "snow/value.h"
#include "snow/type.h"
#include "snow/object.h"
#include "snow/process.h"
#include "snow/function.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/boolean.h"
#include "snow/exception.h"

static VALUE numeric_add(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (!it) return self;
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

static VALUE numeric_subtract(SnFunctionCallContext* here, VALUE self, VALUE it) {
	const bool is_self_int = snow_is_integer(self);
	if (!it) {
		// unary
		if (is_self_int) return snow_integer_to_value(-snow_value_to_integer(self));
		else return snow_float_to_value(-snow_value_to_float(self));
	} else {
		// binary
		const bool is_it_int = snow_is_integer(it);
		if (is_self_int && is_it_int) {
			return snow_integer_to_value(snow_value_to_integer(self) - snow_value_to_integer(it));
		} else {
			float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
			float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
			return snow_float_to_value(a-b);
		}
	}
}

static VALUE numeric_multiply(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (!it) return self;
	const SnType self_type = snow_type_of(self);
	const SnType it_type = snow_type_of(it);
	const bool is_self_int = self_type == SnIntegerType;
	const bool is_it_int = it_type == SnIntegerType;
	if (is_self_int && is_it_int) {
		return snow_integer_to_value(snow_value_to_integer(self) * snow_value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != SnFloatType) return NULL;
		if (!is_it_int && it_type != SnFloatType) return NULL;
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_float_to_value(a*b);
	}
}

static VALUE numeric_divide(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnType self_type = snow_type_of(self);
	const SnType it_type = snow_type_of(it);
	const bool is_self_int = self_type == SnIntegerType;
	const bool is_it_int = it_type == SnIntegerType;
	if (is_self_int && is_it_int) {
		return snow_integer_to_value(snow_value_to_integer(self) / snow_value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != SnFloatType) return NULL;
		if (!is_it_int && it_type != SnFloatType) return NULL;
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_float_to_value(a/b);
	}
}

static VALUE numeric_less_than(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnType self_type = snow_type_of(self);
	const SnType it_type = snow_type_of(it);
	const bool is_self_int = self_type == SnIntegerType;
	const bool is_it_int = it_type == SnIntegerType;
	if (is_self_int && is_it_int) {
		return snow_boolean_to_value(snow_value_to_integer(self) < snow_value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != SnFloatType) return NULL;
		if (!is_it_int && it_type != SnFloatType) return NULL;
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_boolean_to_value(a < b);
	}
}

static VALUE numeric_less_than_or_equal(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnType self_type = snow_type_of(self);
	const SnType it_type = snow_type_of(it);
	const bool is_self_int = self_type == SnIntegerType;
	const bool is_it_int = it_type == SnIntegerType;
	if (is_self_int && is_it_int) {
		return snow_boolean_to_value(snow_value_to_integer(self) <= snow_value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != SnFloatType) return NULL;
		if (!is_it_int && it_type != SnFloatType) return NULL;
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_boolean_to_value(a <= b);
	}
}

static VALUE numeric_greater_than(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnType self_type = snow_type_of(self);
	const SnType it_type = snow_type_of(it);
	const bool is_self_int = self_type == SnIntegerType;
	const bool is_it_int = it_type == SnIntegerType;
	if (is_self_int && is_it_int) {
		return snow_boolean_to_value(snow_value_to_integer(self) > snow_value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != SnFloatType) return NULL;
		if (!is_it_int && it_type != SnFloatType) return NULL;
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_boolean_to_value(a > b);
	}
}

static VALUE numeric_greater_than_or_equal(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnType self_type = snow_type_of(self);
	const SnType it_type = snow_type_of(it);
	const bool is_self_int = self_type == SnIntegerType;
	const bool is_it_int = it_type == SnIntegerType;
	if (is_self_int && is_it_int) {
		return snow_boolean_to_value(snow_value_to_integer(self) >= snow_value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != SnFloatType) return NULL;
		if (!is_it_int && it_type != SnFloatType) return NULL;
		float a = is_self_int ? (float)snow_value_to_integer(self) : snow_value_to_float(self);
		float b = is_it_int ? (float)snow_value_to_integer(it) : snow_value_to_float(it);
		return snow_boolean_to_value(a >= b);
	}
}

static VALUE integer_complement(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (!snow_is_integer(self)) return NULL;
	return snow_integer_to_value(~snow_value_to_integer(self));
}

static VALUE integer_modulo(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (!snow_is_integer(self)) return NULL;
	if (!snow_is_integer(it)) {
		snow_throw_exception_with_description("Error in modulo operation: %s is not an integer.", snow_value_to_cstr(it));
		return NULL;
	}
	return snow_integer_to_value(snow_value_to_integer(self) % snow_value_to_integer(it));
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
	SN_DEFINE_METHOD(proto, "-", numeric_subtract, 1);
	SN_DEFINE_METHOD(proto, "*", numeric_multiply, 1);
	SN_DEFINE_METHOD(proto, "/", numeric_divide, 1);
	SN_DEFINE_METHOD(proto, "~", integer_complement, 0);
	SN_DEFINE_METHOD(proto, "%", integer_modulo, 1);
	SN_DEFINE_METHOD(proto, "<", numeric_less_than, 1);
	SN_DEFINE_METHOD(proto, "<=", numeric_less_than_or_equal, 1);
	SN_DEFINE_METHOD(proto, ">", numeric_greater_than, 1);
	SN_DEFINE_METHOD(proto, ">=", numeric_greater_than_or_equal, 1);
	SN_DEFINE_METHOD(proto, "inspect", numeric_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", numeric_inspect, 0);
	return proto;
}

SnObject* snow_create_float_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "+", numeric_add, 1);
	SN_DEFINE_METHOD(proto, "-", numeric_subtract, 1);
	SN_DEFINE_METHOD(proto, "*", numeric_multiply, 1);
	SN_DEFINE_METHOD(proto, "/", numeric_divide, 1);
	SN_DEFINE_METHOD(proto, "<", numeric_less_than, 1);
	SN_DEFINE_METHOD(proto, "<=", numeric_less_than_or_equal, 1);
	SN_DEFINE_METHOD(proto, ">", numeric_greater_than, 1);
	SN_DEFINE_METHOD(proto, ">=", numeric_greater_than_or_equal, 1);
	SN_DEFINE_METHOD(proto, "inspect", numeric_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", numeric_inspect, 0);
	return proto;
}
