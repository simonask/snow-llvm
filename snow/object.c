#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"
#include "snow/map.h"

struct SnArguments;

typedef struct SnObjectData {
	SnMapRef members;
	SnMapRef properties;
} SnObjectData;

static void object_initialize(SnProcess* p, SnObject* o) {
	
}

static void object_finalize(SnProcess* p, SnObject* o) {
	
}

static void object_copy(SnProcess* p, SnObject* copy, const SnObject* original) {
	
}

static void object_for_each_root(SnProcess* p, VALUE o, SnForEachRootCallbackFunc callback, void* userdata) {
	
}

static VALUE object_get_member(SnProcess* p, VALUE self, SnSymbol sym) {
	return NULL;
}

static VALUE object_set_member(SnProcess* p, VALUE self, SnSymbol sym, VALUE val) {
	return NULL;
}

static VALUE object_call(SnProcess* p, VALUE functor, VALUE self, struct SnArguments* args) {
	return NULL;
}


static SnType _type;

const SnType* snow_get_object_type() {
	static SnType* type = NULL;
	if (type == NULL) {
		type = &_type;
		type->size = sizeof(SnObjectData);
		type->initialize = object_initialize;
		type->finalize = object_finalize;
		type->copy = object_copy;
		type->for_each_root = object_for_each_root;
		type->get_member = object_get_member;
		type->set_member = object_set_member;
		type->call = object_call;
	}
	return type;
}

VALUE snow_object_get_member(SN_P p, SnObject* obj, VALUE self, SnSymbol member) {
	return SN_NIL;
}

VALUE snow_object_set_member(SN_P p, SnObject* obj, VALUE self, SnSymbol member, VALUE value) {
	return SN_NIL;
}
