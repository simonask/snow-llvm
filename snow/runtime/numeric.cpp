#include "snow/numeric.hpp"
#include "internal.h"
#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/exception.hpp"
#include "snow/function.hpp"
#include "snow/object.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/value.hpp"

using namespace snow;

static VALUE numeric_add(const CallFrame* here, VALUE self, VALUE it) {
	if (!it) return self;
	const bool is_self_int = is_integer(self);
	const bool is_it_int = is_integer(it);
	if (is_self_int && is_it_int) {
		return integer_to_value(value_to_integer(self) + value_to_integer(it));
	} else {
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return float_to_value(a + b);
	}
}

static VALUE numeric_subtract(const CallFrame* here, VALUE self, VALUE it) {
	const bool is_self_int = is_integer(self);
	if (!it) {
		// unary
		if (is_self_int) return integer_to_value(-value_to_integer(self));
		else return float_to_value(-value_to_float(self));
	} else {
		// binary
		const bool is_it_int = is_integer(it);
		if (is_self_int && is_it_int) {
			return integer_to_value(value_to_integer(self) - value_to_integer(it));
		} else {
			float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
			float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
			return float_to_value(a-b);
		}
	}
}

static VALUE numeric_multiply(const CallFrame* here, VALUE self, VALUE it) {
	if (!it) return self;
	const ValueType self_type = type_of(self);
	const ValueType it_type = type_of(it);
	const bool is_self_int = self_type == IntegerType;
	const bool is_it_int = it_type == IntegerType;
	if (is_self_int && is_it_int) {
		return integer_to_value(value_to_integer(self) * value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != FloatType) return NULL;
		if (!is_it_int && it_type != FloatType) return NULL;
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return float_to_value(a*b);
	}
}

static VALUE numeric_divide(const CallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const ValueType self_type = type_of(self);
	const ValueType it_type = type_of(it);
	const bool is_self_int = self_type == IntegerType;
	const bool is_it_int = it_type == IntegerType;
	if (is_self_int && is_it_int) {
		return integer_to_value(value_to_integer(self) / value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != FloatType) return NULL;
		if (!is_it_int && it_type != FloatType) return NULL;
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return float_to_value(a/b);
	}
}

static VALUE numeric_less_than(const CallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const ValueType self_type = type_of(self);
	const ValueType it_type = type_of(it);
	const bool is_self_int = self_type == IntegerType;
	const bool is_it_int = it_type == IntegerType;
	if (is_self_int && is_it_int) {
		return boolean_to_value(value_to_integer(self) < value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != FloatType) return NULL;
		if (!is_it_int && it_type != FloatType) return NULL;
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return boolean_to_value(a < b);
	}
}

static VALUE numeric_less_than_or_equal(const CallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const ValueType self_type = type_of(self);
	const ValueType it_type = type_of(it);
	const bool is_self_int = self_type == IntegerType;
	const bool is_it_int = it_type == IntegerType;
	if (is_self_int && is_it_int) {
		return boolean_to_value(value_to_integer(self) <= value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != FloatType) return NULL;
		if (!is_it_int && it_type != FloatType) return NULL;
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return boolean_to_value(a <= b);
	}
}

static VALUE numeric_greater_than(const CallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const ValueType self_type = type_of(self);
	const ValueType it_type = type_of(it);
	const bool is_self_int = self_type == IntegerType;
	const bool is_it_int = it_type == IntegerType;
	if (is_self_int && is_it_int) {
		return boolean_to_value(value_to_integer(self) > value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != FloatType) return NULL;
		if (!is_it_int && it_type != FloatType) return NULL;
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return boolean_to_value(a > b);
	}
}

static VALUE numeric_greater_than_or_equal(const CallFrame* here, VALUE self, VALUE it) {
	ASSERT(it); // TODO: Exception
	const ValueType self_type = type_of(self);
	const ValueType it_type = type_of(it);
	const bool is_self_int = self_type == IntegerType;
	const bool is_it_int = it_type == IntegerType;
	if (is_self_int && is_it_int) {
		return boolean_to_value(value_to_integer(self) >= value_to_integer(it));
	} else {
		// TODO: Automatic conversion?
		if (!is_self_int && self_type != FloatType) return NULL;
		if (!is_it_int && it_type != FloatType) return NULL;
		float a = is_self_int ? (float)value_to_integer(self) : value_to_float(self);
		float b = is_it_int ? (float)value_to_integer(it) : value_to_float(it);
		return boolean_to_value(a >= b);
	}
}

static VALUE integer_complement(const CallFrame* here, VALUE self, VALUE it) {
	if (!is_integer(self)) return NULL;
	return integer_to_value(~value_to_integer(self));
}

static VALUE integer_modulo(const CallFrame* here, VALUE self, VALUE it) {
	if (!is_integer(self)) return NULL;
	if (!is_integer(it)) {
		throw_exception_with_description("Error in modulo operation: %p is not an integer.", it);
		return NULL;
	}
	return integer_to_value(value_to_integer(self) % value_to_integer(it));
}

static VALUE numeric_inspect(const CallFrame* here, VALUE self, VALUE it) {
	if (is_integer(self)) {
		char buffer[100];
		snprintf(buffer, 100, "%d", value_to_integer(self));
		return snow::create_string(buffer);
	} else if (is_float(self)) {
		char buffer[100];
		snprintf(buffer, 100, "%f", value_to_float(self));
		return snow::create_string(buffer);
	}
	return SN_NIL;
}

namespace snow {
ObjectPtr<Class> get_numeric_class() {
	static SnObject** root = NULL;
	if (!root) {
		ObjectPtr<Class> cls = create_class(snow::sym("Numeric"), NULL);
		SN_DEFINE_METHOD(cls, "+", numeric_add);
		SN_DEFINE_METHOD(cls, "-", numeric_subtract);
		SN_DEFINE_METHOD(cls, "*", numeric_multiply);
		SN_DEFINE_METHOD(cls, "/", numeric_divide);
		SN_DEFINE_METHOD(cls, "<", numeric_less_than);
		SN_DEFINE_METHOD(cls, "<=", numeric_less_than_or_equal);
		SN_DEFINE_METHOD(cls, ">", numeric_greater_than);
		SN_DEFINE_METHOD(cls, ">=", numeric_greater_than_or_equal);
		SN_DEFINE_METHOD(cls, "inspect", numeric_inspect);
		SN_DEFINE_METHOD(cls, "to_string", numeric_inspect);
		root = gc_create_root(cls);
	}
	return *root;
}

ObjectPtr<Class> get_float_class() {
	static SnObject** root = NULL;
	if (!root) {
		ObjectPtr<Class> cls = create_class(snow::sym("Float"), get_numeric_class());
		root = gc_create_root(cls);
	}
	return *root;
}

ObjectPtr<Class> get_integer_class() {
	static SnObject** root = NULL;
	if (!root) {
		ObjectPtr<Class> cls = create_class(snow::sym("Integer"), get_numeric_class());
		SN_DEFINE_METHOD(cls, "%", integer_modulo);
		SN_DEFINE_METHOD(cls, "~", integer_complement);
		root = gc_create_root(cls);
	}
	return *root;
}
}
