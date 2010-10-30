#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"
#include "snow/map.h"

struct SnArguments;

VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol member) {
	return SN_NIL;
}

VALUE snow_object_set_member(SnObject* obj, VALUE self, SnSymbol member, VALUE value) {
	return SN_NIL;
}
