#include "snow/array.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/util.hpp"
#include "snow/objectptr.hpp"
#include "snow/exception.hpp"
#include "snow/function.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/numeric.hpp"
#include "snow/type.hpp"

#include <string.h>
#include <vector>

namespace snow {
	struct Array : std::vector<Value> {
		Array() {}
		Array(const Array& other) : std::vector<Value>(other) {}
	};
	
	SN_REGISTER_CPP_TYPE(Array, NULL);

	ObjectPtr<Array> create_array() {
		return create_object(get_array_class(), 0, NULL);
	}

	ObjectPtr<Array> create_array_with_size(uint32_t sz) {
		ObjectPtr<Array> array = create_array();
		array_reserve(array, sz);
		return array;
	}

	ObjectPtr<Array> create_array_from_range(const Value* begin, const Value* end) {
		ASSERT(begin <= end);
		int32_t sz = (int32_t)(end - begin);
		return create_object(get_array_class(), sz, begin);
	}
	
	size_t array_size(ArrayConstPtr array) {
		return array->size();
	}

	Value array_get(ArrayConstPtr array, int32_t idx) {
		if (idx >= array->size())
			return NULL;
		if (idx < 0)
			idx += array->size();
		if (idx < 0)
			return NULL;
		return (*array)[idx];
	}

	size_t array_get_all(ArrayConstPtr array, Value* out_values, size_t max) {		
		size_t i;
		for (i = 0; i < array->size() && i < max; ++i) {
			out_values[i] = (*array)[i];
		}
		return i;
	}

	Value& array_set(ArrayPtr array, int idx, Value val) {
		if (idx >= array->size())
			array->resize(idx+1, NULL);
		if (idx < 0)
			idx += array->size();
		if (idx < 0) {
			throw_exception_with_description("Index %d is out of bounds.", idx - array->size());
		}
		Value& ref = (*array)[idx];
		ref = val;
		return ref;
	}

	void array_reserve(ArrayPtr array, uint32_t new_size) {
		array->reserve(new_size);
	}

	void array_push(ArrayPtr array, Value val) {
		array->push_back(val);
	}

	bool array_contains(ArrayConstPtr array, Value val) {
		return array_index_of(array, val) >= 0;
	}
	
	int32_t array_index_of(ArrayConstPtr array, Value val) {
		return snow::index_of(*array, val);
	}

	namespace bindings {
		VALUE array_initialize(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Array> array = self;
			if (array == NULL) {
				throw_exception_with_description("Array#initialize called for object that doesn't derive from Array.");
			}

			array->reserve(here->args->size);
			array->insert(array->begin(), here->args->data, here->args->data + here->args->size);
			return self;
		}

		VALUE array_inspect(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Array> array = self;
			if (array == NULL) {
				throw_exception_with_description("Array#inspect called for object that doesn't derive from Array.");
			}

			ObjectPtr<String> result = create_string_constant("@(");

			for (size_t i = 0; i < array->size(); ++i) {
				Value val = (*array)[i];
				string_append(result, value_inspect(val));
				if (i != array->size() - 1) {
					string_append_cstr(result, ", ");
				}
			}

			string_append_cstr(result, ")");

			return result;
		}

		VALUE array_index_get(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Array> array = self;
			if (array == NULL) return NULL;
			if (type_of(it) != IntegerType) {
				throw_exception_with_description("Array#get called with a non-integer index %p.", it);
			}
			return array_get(array, value_to_integer(it));
		}

		VALUE array_index_set(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Array> array = self;
			if (array == NULL) return NULL;
			if (type_of(it) != IntegerType) {
				throw_exception_with_description("Array#set called with a non-integer index %p.", it);
			}
			Value val = here->args->size > 1 ? here->args->data[1] : Value(SN_NIL);
			return array_set(array, value_to_integer(it), val);
		}

		VALUE array_multiply_or_splat(const CallFrame* here, VALUE self, VALUE it) {
			if (!it) return self;
			return self; // TODO: Something useful?
		}

		VALUE array_each(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Array> array = self;
			if (array == NULL) return NULL;
			for (size_t i = 0; i < array->size(); ++i) {
				Value args[] = { (*array)[i] };
				call(it, NULL, 1, args);
			}
			return SN_NIL;
		}

		VALUE array_push(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Array> array = self;
			if (array == NULL) return NULL;
		// TODO: Check for recursion?
			array_push(array, it);
			return self;
		}

		VALUE array_get_size(const CallFrame* here, VALUE self, VALUE it) {
			if (!snow::value_is_of_type(self, get_type<Array>())) return NULL;
			return integer_to_value(array_size(self));
		}
	}

	ObjectPtr<Class> get_array_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Array"), get_type<Array>());
			SN_DEFINE_METHOD(cls, "initialize", bindings::array_initialize);
			SN_DEFINE_METHOD(cls, "inspect", bindings::array_inspect);
			SN_DEFINE_METHOD(cls, "to_string", bindings::array_inspect);
			SN_DEFINE_METHOD(cls, "get", bindings::array_index_get);
			SN_DEFINE_METHOD(cls, "set", bindings::array_index_set);
			SN_DEFINE_METHOD(cls, "*", bindings::array_multiply_or_splat);
			SN_DEFINE_METHOD(cls, "each", bindings::array_each);
			SN_DEFINE_METHOD(cls, "push", bindings::array_push);
			SN_DEFINE_METHOD(cls, "<<", bindings::array_push);
			SN_DEFINE_PROPERTY(cls, "size", bindings::array_get_size, NULL);
			root = gc_create_root(cls);
		}
		return *root;
	}
}


