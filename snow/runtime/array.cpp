#include "snow/array.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "util.hpp"
#include "objectptr.hpp"
#include "snow/exception.h"
#include "snow/function.hpp"
#include "snow/snow.h"
#include "snow/str.hpp"
#include "snow/numeric.h"
#include "snow/type.hpp"

#include <string.h>
#include <vector>

namespace {
	typedef std::vector<VALUE> Array;
	
	void array_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		Array& priv = *(Array*)data;
		for (size_t i = 0; i < priv.size(); ++i) {
			callback(&priv[i]);
		}
	}
}

SN_REGISTER_CPP_TYPE(Array, array_gc_each_root);

CAPI {
	using namespace snow;
		
	SnObject* snow_create_array() {
		return snow_create_object(snow_get_array_class(), 0, NULL);
	}

	SnObject* snow_create_array_with_size(uint32_t sz) {
		SnObject* array = snow_create_array();
		snow_array_reserve(array, sz);
		return array;
	}

	SnObject* snow_create_array_from_range(VALUE* begin, VALUE* end) {
		ASSERT(begin <= end);
		int32_t sz = (int32_t)(end - begin);
		return snow_create_object(snow_get_array_class(), sz, begin);
	}
	
	size_t snow_array_size(const SnObject* array) {
		return ObjectPtr<const Array>(array)->size();
	}

	VALUE snow_array_get(const SnObject* array, int idx) {
		ObjectPtr<const Array> a = array;
		ASSERT(a != NULL);
		
		if (idx >= a->size())
			return NULL;
		if (idx < 0)
			idx += a->size();
		if (idx < 0)
			return NULL;
		return (*a)[idx];
	}

	size_t snow_array_get_all(const SnObject* array, VALUE* out_values, size_t max) {
		ObjectPtr<const Array> a = array;
		ASSERT(a != NULL);
		
		size_t i;
		for (i = 0; i < a->size() && i < max; ++i) {
			out_values[i] = (*a)[i];
		}
		return i;
	}

	VALUE snow_array_set(SnObject* array, int idx, VALUE val) {
		ObjectPtr<Array> a = array;
		ASSERT(a != NULL);
		if (idx >= a->size())
			a->resize(idx+1, NULL);
		if (idx < 0)
			idx += a->size();
		if (idx < 0) {
			snow_throw_exception_with_description("Index %d is out of bounds.", idx - a->size());
			return NULL;
		}
		(*a)[idx] = val;
		return val;
	}

	void snow_array_reserve(SnObject* array, uint32_t new_size) {
		ObjectPtr<Array>(array)->reserve(new_size);
	}

	SnObject* snow_array_push(SnObject* array, VALUE val) {
		ObjectPtr<Array>(array)->push_back(val);
		return array;
	}

	bool snow_array_contains(const SnObject* array, VALUE val) {
		return snow_array_index_of(array, val) >= 0;
	}
	
	int32_t snow_array_index_of(const SnObject* a, VALUE val) {
		ObjectPtr<const Array> array = a;
		return snow::index_of(*array, val);
	}
}

namespace {
	VALUE array_initialize(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Array> array = self;
		if (array == NULL) {
			snow_throw_exception_with_description("Array#initialize called for object that doesn't derive from Array.");
		}
		
		array->reserve(here->args->size);
		array->insert(array->begin(), here->args->data, here->args->data + here->args->size);
		return self;
	}
	
	VALUE array_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Array> array = self;
		if (array == NULL) {
			snow_throw_exception_with_description("Array#inspect called for object that doesn't derive from Array.");
		}
		
		SnObject* result = snow_create_string_constant("@(");
		
		for (size_t i = 0; i < array->size(); ++i) {
			VALUE val = (*array)[i];
			snow_string_append(result, snow_value_inspect(val));
			if (i != array->size() - 1) {
				snow_string_append_cstr(result, ", ");
			}
		}
		
		snow_string_append_cstr(result, ")");
		
		return result;
	}

	VALUE array_index_get(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Array> array = self;
		if (array == NULL) return NULL;
		if (snow_type_of(it) != SnIntegerType) {
			snow_throw_exception_with_description("Array#get called with a non-integer index %p.", it);
		}
		return snow_array_get((SnObject*)self, snow_value_to_integer(it));
	}

	VALUE array_index_set(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Array> array = self;
		if (array == NULL) return NULL;
		if (snow_type_of(it) != SnIntegerType) {
			snow_throw_exception_with_description("Array#set called with a non-integer index %p.", it);
		}
		VALUE val = here->args->size > 1 ? here->args->data[1] : SN_NIL;
		return snow_array_set((SnObject*)self, snow_value_to_integer(it), val);
	}

	VALUE array_multiply_or_splat(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!it) return self;
		return self; // TODO: Something useful?
	}

	VALUE array_each(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Array> array = self;
		if (array == NULL) return NULL;
		for (size_t i = 0; i < array->size(); ++i) {
			VALUE args[] = { (*array)[i] };
			snow_call(it, NULL, 1, args);
		}
		return SN_NIL;
	}

	VALUE array_push(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow::value_is_of_type(self, get_type<Array>())) return NULL;
		// TODO: Check for recursion?
		snow_array_push((SnObject*)self, it);
		return self;
	}

	VALUE array_get_size(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow::value_is_of_type(self, get_type<Array>())) return NULL;
		return snow_integer_to_value(snow_array_size((SnObject*)self));
	}
}

CAPI SnObject* snow_get_array_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class_for_type(snow_sym("Array"), get_type<Array>());
		snow_class_define_method(cls, "initialize", array_initialize);
		snow_class_define_method(cls, "inspect", array_inspect);
		snow_class_define_method(cls, "to_string", array_inspect);
		snow_class_define_method(cls, "get", array_index_get);
		snow_class_define_method(cls, "set", array_index_set);
		snow_class_define_method(cls, "*", array_multiply_or_splat);
		snow_class_define_method(cls, "each", array_each);
		snow_class_define_method(cls, "push", array_push);
		snow_class_define_method(cls, "<<", array_push);
		snow_class_define_property(cls, "size", array_get_size, NULL);
		root = snow_gc_create_root(cls);
	}
	return *root;
}
