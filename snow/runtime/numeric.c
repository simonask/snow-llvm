#include "snow/numeric.h"
#include "internal.h"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/object.h"
#include "snow/process.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/value.h"


static VALUE numeric_add(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_subtract(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_multiply(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_divide(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_less_than(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_less_than_or_equal(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_greater_than(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_greater_than_or_equal(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE integer_complement(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(self)) return NULL;
	return snow_integer_to_value(~snow_value_to_integer(self));
}

static VALUE integer_modulo(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(self)) return NULL;
	if (!snow_is_integer(it)) {
		snow_throw_exception_with_description("Error in modulo operation: %s is not an integer.", snow_value_to_cstr(it));
		return NULL;
	}
	return snow_integer_to_value(snow_value_to_integer(self) % snow_value_to_integer(it));
}

static VALUE numeric_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

SnClass* snow_get_numeric_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("+", numeric_add, 1),
			SN_METHOD("-", numeric_subtract, 1),
			SN_METHOD("*", numeric_multiply, 1),
			SN_METHOD("/", numeric_divide, 1),
			SN_METHOD("<", numeric_less_than, 1),
			SN_METHOD("<=", numeric_less_than_or_equal, 1),
			SN_METHOD(">", numeric_greater_than, 1),
			SN_METHOD(">=", numeric_greater_than_or_equal, 1),
			SN_METHOD("inspect", numeric_inspect, 0),
			SN_METHOD("to_string", numeric_inspect, 0),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Numeric"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}

SnClass* snow_get_float_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnClass* cls = snow_define_class(snow_sym("Float"), snow_get_numeric_class(), 0, NULL, 0, NULL);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}

SnClass* snow_get_integer_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("%", integer_modulo, 1),
			SN_METHOD("~", integer_complement, 0),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Integer"), snow_get_numeric_class(), 0, NULL, countof(methods), NULL);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}
