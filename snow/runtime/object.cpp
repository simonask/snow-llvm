#include "snow/object.hpp"
#include "snow/class.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/boolean.hpp"
#include "snow/numeric.hpp"
#include "snow/exception.h"

#include "snow/util.hpp"

namespace {
	using namespace snow;
	
	VALUE object_inspect(const CallFrame* here, VALUE self, VALUE it) {
		return snow::string_format("[%s@%p]", class_get_name(get_class(self)), self);
	}

	VALUE object_instance_eval(const CallFrame* here, VALUE self, VALUE it) {
		VALUE functor = it;
		return call(functor, self, 0, NULL);
	}

	VALUE object_equals(const CallFrame* here, VALUE self, VALUE it) {
		return snow_boolean_to_value(self == it);
	}

	VALUE object_not_equals(const CallFrame* here, VALUE self, VALUE it) {
		return snow_boolean_to_value(self != it);
	}

 	VALUE object_compare(const CallFrame* here, VALUE self, VALUE it) {
		// XXX: TODO: With accurate GC, this is unsafe.
		return snow_integer_to_value((ssize_t)self - (ssize_t)it);
	}
}

CAPI {
	using namespace snow;
	
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
			int32_t idx = class_get_index_of_instance_variable(cls, name);
			if (idx < 0)
				idx = class_define_instance_variable(cls, name);
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
			int32_t sz = class_get_num_instance_variables(cls);
			ASSERT(idx < sz); // trying to set instance variable not defined in class
			obj->members = snow::realloc_range(obj->members, obj->num_alloc_members, sz);
			obj->num_alloc_members = sz;
		}
		obj->members[idx] = val;
		return val;
	}
	
	int32_t snow_object_get_index_of_instance_variable(const SnObject* obj, SnSymbol name) {
		SnObject* cls = snow_object_get_class(obj);
		return class_get_index_of_instance_variable(cls, name);
	}
	
	bool snow_object_give_meta_class(SnObject* obj) {
		SnObject* cls = snow_object_get_class(obj);
		if (!class_is_meta(cls)) {
			obj->cls = create_meta_class(cls);
			return true;
		}
		return false;
	}
	
	VALUE _snow_object_define_method(SnObject* obj, SnSymbol name, VALUE func) {
		snow_object_give_meta_class(obj);
		snow::_class_define_method(snow_object_get_class(obj), name, func);
		return obj;
	}
	
	VALUE _snow_object_define_property(SnObject* obj, SnSymbol name, VALUE getter, VALUE setter) {
		snow_object_give_meta_class(obj);
		snow::_class_define_property(snow_object_get_class(obj), name, getter, setter);
		return obj;
	}
	
	VALUE snow_object_set_property_or_define_method(SnObject* obj, SnSymbol name, VALUE val) {
		Method method_or_property;
		SnObject* cls = snow_object_get_class(obj);
		if (class_lookup_method(cls, name, &method_or_property)) {
			if (method_or_property.type == MethodTypeProperty) {
				if (method_or_property.property->setter) {
					return call(method_or_property.property->setter, obj, 1, &val);
				}
				snow_throw_exception_with_description("Property '%s' is not writable on objects of class %s.", snow_sym_to_cstr(name), class_get_name(cls));
			}
		}

		_snow_object_define_method(obj, name, val);
		return val;
	}
	
	SnObject* snow_get_object_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = create_class(snow_sym("Object"), NULL);
			SN_DEFINE_METHOD(cls, "inspect", object_inspect);
			SN_DEFINE_METHOD(cls, "to_string", object_inspect);
			SN_DEFINE_METHOD(cls, "instance_eval", object_instance_eval);
			SN_DEFINE_METHOD(cls, "=", object_equals);
			SN_DEFINE_METHOD(cls, "!=", object_not_equals);
			SN_DEFINE_METHOD(cls, "<=>", object_compare);
			root = snow_gc_create_root(cls);
		}
		return *root;
	}
}
