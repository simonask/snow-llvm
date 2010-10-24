#include "snow/numeric.h"
#include "snow/value.h"
#include "snow/type.h"
#include "snow/object.h"
#include "snow/process.h"
#include "snow/snow.h"

static SnType IntegerType;

static void init_integer_type(SN_P);

static VALUE integer_get_member_fast(SN_P p, VALUE self, SnSymbol member) {
	return snow_object_get_member(p, p->immediate_prototypes[snow_get_immediate_type_index(SnIntegerType)], self, member);
}

static VALUE integer_get_member(SN_P p, VALUE self, SnSymbol member) {
	init_integer_type(p);
	return integer_get_member_fast(p, self, member);
}

static VALUE integer_call_fast(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	return snow_call_with_self(p, p->immediate_prototypes[snow_get_immediate_type_index(SnIntegerType)], self, args);
}

static VALUE integer_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	init_integer_type(p);
	return integer_call_fast(p, functor, self, args);
}


static void init_integer_type(SN_P p) {
	IntegerType.get_member = integer_get_member_fast;
	IntegerType.call = integer_call_fast;
}

const SnType* snow_get_integer_type() {
	static const SnType* type = NULL;
	if (!type) {
		snow_init_immediate_type(&IntegerType);
		IntegerType.get_member = integer_get_member;
		IntegerType.call = integer_call;
		type = &IntegerType;
	}
	return type;
}



static VALUE float_get_member(SN_P p, VALUE self, SnSymbol member) {
	return SN_NIL;
}

static VALUE float_set_member(SN_P p, VALUE self, SnSymbol member, VALUE val) {
	return SN_NIL;
}

static VALUE float_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	return functor;
}

static SnType FloatType;

const SnType* snow_get_float_type() {
	static const SnType* type = NULL;
	if (!type) {
		snow_init_immediate_type(&FloatType);
		FloatType.get_member = integer_get_member;
		FloatType.call = integer_call;
		type = &FloatType;
	}
	return type;
}