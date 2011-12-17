#include "snow/object.hpp"
#include "snow/class.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/snow.h"
#include "snow/str.hpp"
#include "snow/boolean.h"
#include "snow/numeric.h"
#include "snow/exception.h"

#include "snow/util.hpp"

namespace {
	VALUE object_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
		return snow_string_format("[%s@%p]", snow_class_get_name(snow_get_class(self)), self);
	}

	VALUE object_instance_eval(const SnCallFrame* here, VALUE self, VALUE it) {
		VALUE functor = it;
		return snow_call(functor, self, 0, NULL);
	}

	VALUE object_equals(const SnCallFrame* here, VALUE self, VALUE it) {
		return snow_boolean_to_value(self == it);
	}

	VALUE object_not_equals(const SnCallFrame* here, VALUE self, VALUE it) {
		return snow_boolean_to_value(self != it);
	}

 	VALUE object_compare(const SnCallFrame* here, VALUE self, VALUE it) {
		// XXX: TODO: With accurate GC, this is unsafe.
		return snow_integer_to_value((ssize_t)self - (ssize_t)it);
	}
}

CAPI {
	VALUE snow_object_get_instance_variable(VALUE obj, SnSymbol name) {
		if (snow_is_object(obj)) {
			int32_t idx = snow_object_get_index_of_instance_variable((SnObject*)obj, name);
			if (idx >= 0) {
				return snow_object_get_instance_variable_by_index((SnObject*)obj, idx);
			}
		}
		return NULL;
	}
	
	VALUE snow_object_set_instance_variable(VALUE obj, SnSymbol name, VALUE val) {
		if (snow_is_object(obj)) {
			SnObject* cls = snow_object_get_class((SnObject*)obj);
			int32_t idx = snow_class_get_index_of_instance_variable(cls, name);
			if (idx < 0)
				idx = snow_class_define_instance_variable(cls, name);
			return snow_object_set_instance_variable_by_index((SnObject*)obj, idx, val);
		} else {
			snow_throw_exception_with_description("Cannot set instance variable of immediate value '%p'.", obj);
			return NULL;
		}
	}
	
	VALUE snow_object_get_instance_variable_by_index(const SnObject* obj, int32_t idx) {
		ASSERT(idx >= 0);
		if (idx > obj->num_alloc_members) return NULL;
		return obj->members[idx];
	}
	
	VALUE snow_object_set_instance_variable_by_index(SnObject* obj, int32_t idx, VALUE val) {
		ASSERT(idx >= 0);
		if (idx >= obj->num_alloc_members) {
			SnObject* cls = snow_object_get_class(obj);
			int32_t sz = snow_class_get_num_instance_variables(cls);
			ASSERT(idx < sz); // trying to set instance variable not defined in class
			obj->members = snow::realloc_range(obj->members, obj->num_alloc_members, sz);
			obj->num_alloc_members = sz;
		}
		obj->members[idx] = val;
		return val;
	}
	
	int32_t snow_object_get_index_of_instance_variable(const SnObject* obj, SnSymbol name) {
		SnObject* cls = snow_object_get_class(obj);
		return snow_class_get_index_of_instance_variable(cls, name);
	}
	
	bool snow_object_give_meta_class(SnObject* obj) {
		SnObject* cls = snow_object_get_class(obj);
		if (!snow_class_is_meta(cls)) {
			obj->cls = snow_create_meta_class(cls);
			return true;
		}
		return false;
	}
	
	VALUE _snow_object_define_method(SnObject* obj, SnSymbol name, VALUE func) {
		snow_object_give_meta_class(obj);
		_snow_class_define_method(snow_object_get_class(obj), name, func);
		return obj;
	}
	
	VALUE _snow_object_define_property(SnObject* obj, SnSymbol name, VALUE getter, VALUE setter) {
		snow_object_give_meta_class(obj);
		_snow_class_define_property(snow_object_get_class(obj), name, getter, setter);
		return obj;
	}
	
	VALUE snow_object_set_property_or_define_method(SnObject* obj, SnSymbol name, VALUE val) {
		SnMethod method_or_property;
		SnObject* cls = snow_object_get_class(obj);
		if (snow_class_lookup_method(cls, name, &method_or_property)) {
			if (method_or_property.type == SnMethodTypeProperty) {
				if (method_or_property.property->setter) {
					return snow_call(method_or_property.property->setter, obj, 1, &val);
				}
				snow_throw_exception_with_description("Property '%s' is not writable on objects of class %s.", snow_sym_to_cstr(name), snow_class_get_name(cls));
			}
		}

		_snow_object_define_method(obj, name, val);
		return val;
	}
	
	SnObject* snow_get_object_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = snow_create_class(snow_sym("Object"), NULL);
			snow_class_define_method(cls, "inspect", object_inspect);
			snow_class_define_method(cls, "to_string", object_inspect);
			snow_class_define_method(cls, "instance_eval", object_instance_eval);
			snow_class_define_method(cls, "=", object_equals);
			snow_class_define_method(cls, "!=", object_not_equals);
			snow_class_define_method(cls, "<=>", object_compare);
			root = snow_gc_create_root(cls);
		}
		return *root;
	}
}
