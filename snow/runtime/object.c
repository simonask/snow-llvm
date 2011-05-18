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
	if (!cls) cls = snow_get_object_class();
	SnObject* obj = snow_gc_alloc_object(cls->internal_type);
	_snow_object_init(obj, cls);
	return obj;
}

void snow_finalize_object(SnObject* obj) {
	free(obj->members);
}

VALUE snow_object_get_field(VALUE obj, SnSymbol name) {
	if (!snow_is_object(obj)) return NULL;
	return snow_object_get_field_by_index((SnObject*)obj, snow_class_get_index_of_field(snow_get_class(obj), name));
}

VALUE snow_object_set_field(VALUE obj, SnSymbol name, VALUE val) {
	if (!snow_is_object(obj)) snow_throw_exception_with_description("Immediate object types cannot have field members!");
	return snow_object_set_field_by_index((SnObject*)obj, snow_class_get_or_define_index_of_field(snow_get_class(obj), name), val);
}

VALUE snow_object_get_field_by_index(const SnObject* obj, int32_t idx) {
	if (idx < 0 || idx >= obj->num_members) return NULL;
	return obj->members[idx];
}

VALUE snow_object_set_field_by_index(SnObject* obj, int32_t idx, VALUE val) {
	ASSERT(idx >= 0);
	if (idx >= obj->num_members) {
		int32_t new_size = idx+1;
		obj->members = (VALUE*)realloc(obj->members, new_size * sizeof(VALUE));
		for (int32_t i = obj->num_members; i < new_size; ++i) {
			obj->members[i] = NULL;
		}
		obj->num_members = new_size;
	}
	obj->members[idx] = val;
	return val;
}

void snow_object_give_meta_class(SnObject* obj) {
	if (!obj->cls->is_meta) {
		obj->cls = snow_create_meta_class(obj->cls);
	}
}

VALUE _snow_object_define_method(VALUE obj, SnSymbol name, VALUE func) {
	if (!snow_is_object(obj)) snow_throw_exception_with_description("Cannot define instance methods on immediate objects.");
	SnObject* o = (SnObject*)obj;
	snow_object_give_meta_class(o);
	if (!_snow_class_define_method(o->cls, name, func)) {
		snow_throw_exception_with_description("Method '%s' is already defined on object %p.", snow_sym_to_cstr(name), obj);
	}
	return func;
}

VALUE snow_object_set_property_or_define_method(VALUE obj, SnSymbol name, VALUE func) {
	SnMethod method_or_property;
	SnClass* cls = snow_get_class(obj);
	if (snow_class_lookup_method_or_property(cls, name, &method_or_property)) {
		if (method_or_property.type == SnMethodTypeProperty) {
			if (method_or_property.property.setter) {
				return snow_call(method_or_property.property.setter, obj, 1, &func);
			}
			snow_throw_exception_with_description("Property '%s' is not writable on objects of class %s.", snow_sym_to_cstr(cls->name));
		}
	}
	
	if (snow_is_object(obj)) {
		_snow_object_define_method(obj, name, func);
		return func;
	}
	
	snow_throw_exception_with_description("Cannot define object methods on immediates of type %s.", snow_sym_to_cstr(cls->name));
	return NULL;
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

static VALUE object_create(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(it) == SnClassType) {
		return snow_create_object((SnClass*)it);
	}
	return snow_create_object(NULL);
}

SnClass* snow_get_object_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnClass* cls = snow_create_class(snow_sym("Object"), NULL);
		snow_class_define_method(cls, "inspect", object_inspect, 0);
		snow_class_define_method(cls, "to_string", object_inspect, 0);
		snow_class_define_method(cls, "instance_eval", object_instance_eval, 1);
		snow_class_define_method(cls, "=", object_equals, 1);
		snow_class_define_method(cls, "!=", object_not_equals, 1);
		snow_class_define_method(cls, "<=>", object_compare, 1);
		snow_object_define_method(cls, "__call__", object_create, 1);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}

