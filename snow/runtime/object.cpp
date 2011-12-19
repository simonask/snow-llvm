#include "snow/object.hpp"
#include "snow/class.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/boolean.hpp"
#include "snow/numeric.hpp"
#include "snow/exception.hpp"

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
		return boolean_to_value(self == it);
	}

	VALUE object_not_equals(const CallFrame* here, VALUE self, VALUE it) {
		return boolean_to_value(self != it);
	}

 	VALUE object_compare(const CallFrame* here, VALUE self, VALUE it) {
		// XXX: TODO: With accurate GC, this is unsafe.
		return integer_to_value((ssize_t)self - (ssize_t)it);
	}
}

namespace snow {
	VALUE object_get_instance_variable(VALUE obj, Symbol name) {
		if (is_object(obj)) {
			int32_t idx = object_get_index_of_instance_variable((SnObject*)obj, name);
			if (idx >= 0) {
				return object_get_instance_variable_by_index((SnObject*)obj, idx);
			}
		}
		return NULL;
	}
	
	VALUE object_set_instance_variable(VALUE obj, Symbol name, VALUE val) {
		if (is_object(obj)) {
			SnObject* cls = object_get_class((SnObject*)obj);
			int32_t idx = class_get_index_of_instance_variable(cls, name);
			if (idx < 0)
				idx = class_define_instance_variable(cls, name);
			return object_set_instance_variable_by_index((SnObject*)obj, idx, val);
		} else {
			throw_exception_with_description("Cannot set instance variable of immediate value '%p'.", obj);
			return NULL;
		}
	}
	
	VALUE object_get_instance_variable_by_index(const SnObject* obj, int32_t idx) {
		ASSERT(idx >= 0);
		if (idx > obj->num_alloc_members) return NULL;
		return obj->members[idx];
	}
	
	VALUE object_set_instance_variable_by_index(SnObject* obj, int32_t idx, VALUE val) {
		ASSERT(idx >= 0);
		if (idx >= obj->num_alloc_members) {
			SnObject* cls = object_get_class(obj);
			int32_t sz = class_get_num_instance_variables(cls);
			ASSERT(idx < sz); // trying to set instance variable not defined in class
			obj->members = snow::realloc_range(obj->members, obj->num_alloc_members, sz);
			obj->num_alloc_members = sz;
		}
		obj->members[idx] = val;
		return val;
	}
	
	int32_t object_get_index_of_instance_variable(const SnObject* obj, Symbol name) {
		SnObject* cls = object_get_class(obj);
		return class_get_index_of_instance_variable(cls, name);
	}
	
	bool object_give_meta_class(SnObject* obj) {
		SnObject* cls = object_get_class(obj);
		if (!class_is_meta(cls)) {
			obj->cls = create_meta_class(cls);
			return true;
		}
		return false;
	}
	
	VALUE _object_define_method(SnObject* obj, Symbol name, VALUE func) {
		object_give_meta_class(obj);
		snow::_class_define_method(object_get_class(obj), name, func);
		return obj;
	}
	
	VALUE _object_define_property(SnObject* obj, Symbol name, VALUE getter, VALUE setter) {
		object_give_meta_class(obj);
		snow::_class_define_property(object_get_class(obj), name, getter, setter);
		return obj;
	}
	
	VALUE object_set_property_or_define_method(SnObject* obj, Symbol name, VALUE val) {
		Method method_or_property;
		SnObject* cls = object_get_class(obj);
		if (class_lookup_method(cls, name, &method_or_property)) {
			if (method_or_property.type == MethodTypeProperty) {
				if (method_or_property.property->setter) {
					return call(method_or_property.property->setter, obj, 1, &val);
				}
				throw_exception_with_description("Property '%s' is not writable on objects of class %s.", snow::sym_to_cstr(name), class_get_name(cls));
			}
		}

		_object_define_method(obj, name, val);
		return val;
	}
	
	SnObject* get_object_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = create_class(snow::sym("Object"), NULL);
			SN_DEFINE_METHOD(cls, "inspect", object_inspect);
			SN_DEFINE_METHOD(cls, "to_string", object_inspect);
			SN_DEFINE_METHOD(cls, "instance_eval", object_instance_eval);
			SN_DEFINE_METHOD(cls, "=", object_equals);
			SN_DEFINE_METHOD(cls, "!=", object_not_equals);
			SN_DEFINE_METHOD(cls, "<=>", object_compare);
			root = gc_create_root(cls);
		}
		return *root;
	}
}
