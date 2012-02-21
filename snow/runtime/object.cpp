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
		return snow::format_string("[%@@%@]", class_get_name(get_class(self)), format::pointer(self));
	}

	VALUE object_instance_eval(const CallFrame* here, VALUE self, VALUE it) {
		return call(it, self, 0, NULL);
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
	
	VALUE object_get_class(const CallFrame* here, VALUE self, VALUE it) {
		return get_class(self);
	}
	
	VALUE object_get_object_id(const CallFrame* here, VALUE self, VALUE it) {
		return integer_to_value((intptr_t)self);
	}
	
	VALUE object_method_missing(const CallFrame* here, VALUE self, VALUE it) {
		if (is_symbol(it)) {
			Symbol sym = value_to_symbol(it);
			throw_exception_with_description("Object %@ does not respond to method '%@'.", value_inspect(self), sym_to_cstr(sym));
		} else {
			throw_exception_with_description("Object#method_missing called with wrong argument '%@'.", value_inspect(it));
		}
		return NULL;
	}
}

namespace snow {
	Value object_get_instance_variable(AnyObjectPtr obj, Symbol name) {
		ASSERT(obj != NULL);
		int32_t idx = object_get_index_of_instance_variable(obj, name);
		if (idx >= 0) {
			return object_get_instance_variable_by_index(obj, idx);
		}
		return NULL;
	}
	
	Value object_set_instance_variable(AnyObjectPtr obj, Symbol name, Value val) {
		ASSERT(obj != NULL);
		int32_t idx = class_get_index_of_instance_variable(obj->cls, name);
		if (idx < 0)
			idx = class_define_instance_variable(obj->cls, name);
		return object_set_instance_variable_by_index(obj, idx, val);
	}
	
	Value object_get_instance_variable_by_index(AnyObjectPtr obj, int32_t idx) {
		if (idx < 0 || idx > obj->num_alloc_members) return NULL;
		return obj->members[idx];
	}
	
	Value& object_set_instance_variable_by_index(AnyObjectPtr obj, int32_t idx, Value val) {
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
	
	int32_t object_get_index_of_instance_variable(AnyObjectPtr obj, Symbol name) {
		return class_get_index_of_instance_variable(obj->cls, name);
	}
	
	int32_t object_get_or_create_index_of_instance_variable(AnyObjectPtr object, Symbol name) {
		int32_t idx = class_get_index_of_instance_variable(object->cls, name);
		if (idx < 0) {
			idx = class_define_instance_variable(object->cls, name);
		}
		ASSERT(idx >= 0);
		return idx;
	}
	
	bool object_give_meta_class(AnyObjectPtr obj) {
		if (!class_is_meta(obj->cls)) {
			obj->cls = create_meta_class(obj->cls);
			return true;
		}
		return false;
	}
	
	Value _object_define_method(AnyObjectPtr obj, Symbol name, Value func) {
		object_give_meta_class(obj);
		snow::_class_define_method(obj->cls, name, func);
		return obj;
	}
	
	Value _object_define_property(AnyObjectPtr obj, Symbol name, Value getter, Value setter) {
		object_give_meta_class(obj);
		snow::_class_define_property(obj->cls, name, getter, setter);
		return obj;
	}
	
	Value object_set_property_or_define_method(AnyObjectPtr obj, Symbol name, Value val) {
		MethodQueryResult method;
		if (class_lookup_property_setter(obj->cls, name, &method)) {
			if (method.result != NULL) {
				return call(method.result, obj, 1, &val);
			} else {
				throw_exception_with_description("Property '%@' is not writable on objects of class %@.", snow::sym_to_cstr(name), class_get_name(obj->cls));
			}
		}

		_object_define_method(obj, name, val);
		return val;
	}
	
	ObjectPtr<Class> get_object_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class(snow::sym("Object"), NULL);
			root = gc_create_root(cls);
			SN_DEFINE_METHOD(cls, "inspect", object_inspect);
			SN_DEFINE_METHOD(cls, "to_string", object_inspect);
			SN_DEFINE_METHOD(cls, "instance_eval", object_instance_eval);
			SN_DEFINE_METHOD(cls, "=", object_equals);
			SN_DEFINE_METHOD(cls, "!=", object_not_equals);
			SN_DEFINE_METHOD(cls, "<=>", object_compare);
			SN_DEFINE_METHOD(cls, "method_missing", object_method_missing);
			//SN_DEFINE_METHOD(cls, "get_missing_property", object_get_missing_property);
			//SN_DEFINE_METHOD(cls, "set_missing_property", object_set_missing_property);
			SN_DEFINE_PROPERTY(cls, "class", object_get_class, NULL);
			SN_DEFINE_PROPERTY(cls, "object_id", object_get_object_id, NULL);
		}
		return *root;
	}
}
