#include "snow/type.h"

static VALUE dummy_set_member(SN_P p, VALUE self, SnSymbol member, VALUE val) {
	// TODO: Exception.
	TRAP(); // Cannot set member of immediate
	return NULL;
}

static VALUE dummy_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	return functor;
}

void snow_init_immediate_type(SnType* type) {
	type->size = SN_TYPE_INVALID_SIZE;
	type->initialize = NULL;
	type->finalize = NULL;
	type->copy = NULL;
	type->for_each_root = NULL;
	type->get_member = NULL;
	type->set_member = dummy_set_member;
	type->call = dummy_call;
}