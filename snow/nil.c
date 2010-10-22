#include "snow/nil.h"
#include "snow/type.h"

static SnType NilType;

static VALUE nil_get_member(SN_P p, VALUE self, SnSymbol member) {
	ASSERT(!self || self == SN_NIL);
	return NULL;
}

static VALUE nil_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	TRAP(); // called NIL !!
	return NULL;
}

const SnType* snow_get_nil_type() {
	static const SnType* type = NULL;
	if (!type) {
		snow_init_immediate_type(&NilType);
		NilType.get_member = nil_get_member;
		NilType.call = nil_call;
		type = &NilType;
	}
	return type;
}