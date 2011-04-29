#include "snow/object.h"
#include "internal.h"
#include "snow/array.h"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/map.h"
#include "snow/numeric.h"
#include "snow/process.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/type.h"

void _snow_object_init(SnObject* obj, SnClass* cls) {
	obj->type = cls->internal_type;
	obj->cls = cls;
	obj->members = NULL;
	obj->num_members = 0;
}

SnObject* snow_create_object(SnClass* cls) {
	ASSERT(cls);
	SnObject* obj = snow_gc_alloc_object(cls->internal_type);
	_snow_object_init(obj, cls);
	return obj;
}

void snow_finalize_object(SnObject* obj) {
	free(obj->members);
}

static VALUE object_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	char buffer[100];
	snprintf(buffer, 100, "[%s@%p]", snow_sym_to_cstr(snow_get_class(self)->name), self);
	return snow_create_string(buffer);
}

static VALUE object_instance_eval(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	VALUE functor = it;
	return snow_call(functor, self, 0, NULL);
}

static VALUE object_equals(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_boolean_to_value(self == it);
}

static VALUE object_not_equals(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	return snow_boolean_to_value(self != it);
}

static VALUE object_compare(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	// XXX: TODO: With accurate GC, this is unsafe.
	return snow_integer_to_value((ssize_t)self - (ssize_t)it);
}

SnClass* snow_get_object_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("inspect", object_inspect, 0),
			SN_METHOD("to_string", object_inspect, 0),
			SN_METHOD("instance_eval", object_instance_eval, 1),
			SN_METHOD("=", object_equals, 1),
			SN_METHOD("!=", object_not_equals, 1),
			SN_METHOD("<=>", object_compare, 1),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Object"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}

