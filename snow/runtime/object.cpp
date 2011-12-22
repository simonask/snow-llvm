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
	
	Value object_inspect(const CallFrame* here, const Value& self, const Value& it) {
		return snow::string_format("[%s@%p]", class_get_name(get_class(self)), self.value());
	}

	Value object_instance_eval(const CallFrame* here, const Value& self, const Value& it) {
		return call(it, self, 0, NULL);
	}

	Value object_equals(const CallFrame* here, const Value& self, const Value& it) {
		return boolean_to_value(self == it);
	}

	Value object_not_equals(const CallFrame* here, const Value& self, const Value& it) {
		return boolean_to_value(self != it);
	}

 	Value object_compare(const CallFrame* here, const Value& self, const Value& it) {
		// XXX: TODO: With accurate GC, this is unsafe.
		return integer_to_value((ssize_t)self.value() - (ssize_t)it.value());
	}
}

namespace snow {
	Value object_get_instance_variable(const AnyObjectPtr& obj, Symbol name) {
		ASSERT(obj != NULL);
		int32_t idx = object_get_index_of_instance_variable(obj, name);
		if (idx >= 0) {
			return object_get_instance_variable_by_index(obj, idx);
		}
		return NULL;
	}
	
	Value object_set_instance_variable(const AnyObjectPtr& obj, Symbol name, const Value& val) {
		ASSERT(obj != NULL);
		int32_t idx = class_get_index_of_instance_variable(obj->cls, name);
		if (idx < 0)
			idx = class_define_instance_variable(obj->cls, name);
		return object_set_instance_variable_by_index(obj, idx, val);
	}
	
	Value object_get_instance_variable_by_index(const AnyObjectPtr& obj, int32_t idx) {
		if (idx < 0 || idx > obj->num_alloc_members) return NULL;
		return obj->members[idx];
	}
	
	Value& object_set_instance_variable_by_index(const AnyObjectPtr& obj, int32_t idx, const Value& val) {
		ASSERT(idx >= 0);
		if (idx >= obj->num_alloc_members) {
			int32_t sz = class_get_num_instance_variables(obj->cls);
			ASSERT(idx < sz); // trying to set instance variable not defined in class
			obj->members = snow::realloc_range(obj->members, obj->num_alloc_members, sz);
			obj->num_alloc_members = sz;
		}
		Value& place = obj->members[idx];
		place = val;
		return place;
	}
	
	int32_t object_get_index_of_instance_variable(const AnyObjectPtr& obj, Symbol name) {
		return class_get_index_of_instance_variable(obj->cls, name);
	}
	
	int32_t object_get_or_create_index_of_instance_variable(const AnyObjectPtr& object, Symbol name) {
		int32_t idx = class_get_index_of_instance_variable(object->cls, name);
		if (idx < 0) {
			idx = class_define_instance_variable(object->cls, name);
		}
		ASSERT(idx >= 0);
		return idx;
	}
	
	bool object_give_meta_class(const AnyObjectPtr& obj) {
		if (!class_is_meta(obj->cls)) {
			obj->cls = create_meta_class(obj->cls);
			return true;
		}
		return false;
	}
	
	Value _object_define_method(const AnyObjectPtr& obj, Symbol name, const Value& func) {
		object_give_meta_class(obj);
		snow::_class_define_method(obj->cls, name, func);
		return obj;
	}
	
	Value _object_define_property(const AnyObjectPtr& obj, Symbol name, const Value& getter, const Value& setter) {
		object_give_meta_class(obj);
		snow::_class_define_property(obj->cls, name, getter, setter);
		return obj;
	}
	
	Value object_set_property_or_define_method(const AnyObjectPtr& obj, Symbol name, const Value& val) {
		Method method_or_property;
		if (class_lookup_method(obj->cls, name, &method_or_property)) {
			if (method_or_property.type == MethodTypeProperty) {
				if (method_or_property.property->setter) {
					return call(method_or_property.property->setter, obj, 1, &val);
				}
				throw_exception_with_description("Property '%s' is not writable on objects of class %s.", snow::sym_to_cstr(name), class_get_name(obj->cls));
			}
		}

		_object_define_method(obj, name, val);
		return val;
	}
	
	ObjectPtr<Class> get_object_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class(snow::sym("Object"), NULL);
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
