#include "snow/numeric.h"
#include "internal.h"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/object.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/value.h"


static VALUE numeric_add(const SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_subtract(const SnCallFrame* here, VALUE self, VALUE it) {
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

static VALUE numeric_multiply(const SnCallFrame* here, VALUE self, VALUE it) {
	if (!it) return self;
	const SnValueType self_type = snow_type_of(self);
	const SnValueType it_type = snow_type_of(it);
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

static VALUE numeric_divide(const SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnValueType self_type = snow_type_of(self);
	const SnValueType it_type = snow_type_of(it);
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

static VALUE numeric_less_than(const SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnValueType self_type = snow_type_of(self);
	const SnValueType it_type = snow_type_of(it);
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

static VALUE numeric_less_than_or_equal(const SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnValueType self_type = snow_type_of(self);
	const SnValueType it_type = snow_type_of(it);
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

static VALUE numeric_greater_than(const SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnValueType self_type = snow_type_of(self);
	const SnValueType it_type = snow_type_of(it);
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

static VALUE numeric_greater_than_or_equal(const SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const SnValueType self_type = snow_type_of(self);
	const SnValueType it_type = snow_type_of(it);
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

static VALUE integer_complement(const SnCallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(self)) return NULL;
	return snow_integer_to_value(~snow_value_to_integer(self));
}

static VALUE integer_modulo(const SnCallFrame* here, VALUE self, VALUE it) {
	if (!snow_is_integer(self)) return NULL;
	if (!snow_is_integer(it)) {
		snow_throw_exception_with_description("Error in modulo operation: %p is not an integer.", it);
		return NULL;
	}
	return snow_integer_to_value(snow_value_to_integer(self) % snow_value_to_integer(it));
}

static VALUE numeric_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
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

SnObject* snow_get_numeric_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class(snow_sym("Numeric"), NULL);
		snow_class_define_method(cls, "+", numeric_add);
		snow_class_define_method(cls, "-", numeric_subtract);
		snow_class_define_method(cls, "*", numeric_multiply);
		snow_class_define_method(cls, "/", numeric_divide);
		snow_class_define_method(cls, "<", numeric_less_than);
		snow_class_define_method(cls, "<=", numeric_less_than_or_equal);
		snow_class_define_method(cls, ">", numeric_greater_than);
		snow_class_define_method(cls, ">=", numeric_greater_than_or_equal);
		snow_class_define_method(cls, "inspect", numeric_inspect);
		snow_class_define_method(cls, "to_string", numeric_inspect);
		root = snow_gc_create_root(cls);
	}
	return *root;
}

SnObject* snow_get_float_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class(snow_sym("Float"), snow_get_numeric_class());
		root = snow_gc_create_root(cls);
	}
	return *root;
}

SnObject* snow_get_integer_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class(snow_sym("Integer"), snow_get_numeric_class());
		snow_class_define_method(cls, "%", integer_modulo);
		snow_class_define_method(cls, "~", integer_complement);
		root = snow_gc_create_root(cls);
	}
	return *root;
}
