#include "snow/array.h"
#include "internal.h"
#include "snow/class.h"
#include "util.hpp"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/numeric.h"

#include <string.h>
#include <vector>

namespace {
	typedef std::vector<VALUE> ArrayPrivate;
	
	void array_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		ArrayPrivate& priv = *(ArrayPrivate*)data;
		for (size_t i = 0; i < priv.size(); ++i) {
			callback(&priv[i]);
		}
	}
}

CAPI {
	SnInternalType SnArrayType = {
		.data_size = sizeof(ArrayPrivate),
		.initialize = snow::construct<ArrayPrivate>,
		.finalize = snow::destruct<ArrayPrivate>,
		.copy = snow::assign<ArrayPrivate>,
		.gc_each_root = array_gc_each_root,
	};
	
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
		const ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(array, SnArrayType);
		ASSERT(priv);
		return priv->size();
	}

	VALUE snow_array_get(const SnObject* array, int idx) {
		const ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(array, SnArrayType);
		ASSERT(priv);
		
		if (idx >= priv->size())
			return NULL;
		if (idx < 0)
			idx += priv->size();
		if (idx < 0)
			return NULL;
		return (*priv)[idx];
	}

	size_t snow_array_get_all(const SnObject* array, VALUE* out_values, size_t max) {
		const ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(array, SnArrayType);
		ASSERT(priv);
		
		size_t i;
		for (i = 0; i < priv->size() && i < max; ++i) {
			out_values[i] = (*priv)[i];
		}
		return i;
	}

	VALUE snow_array_set(SnObject* array, int idx, VALUE val) {
		ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(array, SnArrayType);
		ASSERT(priv);
		if (idx >= priv->size())
			priv->resize(idx+1, NULL);
		if (idx < 0)
			idx += priv->size();
		if (idx < 0) {
			snow_throw_exception_with_description("Index %d is out of bounds.", idx-priv->size());
			return NULL;
		}
		(*priv)[idx] = val;
		return val;
	}

	void snow_array_reserve(SnObject* array, uint32_t new_size) {
		ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(array, SnArrayType);
		ASSERT(priv);
		priv->reserve(new_size);
	}

	SnObject* snow_array_push(SnObject* array, VALUE val) {
		ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(array, SnArrayType);
		ASSERT(priv);
		priv->push_back(val);
		return array;
	}

	bool snow_array_contains(const SnObject* array, VALUE val) {
		return snow_array_index_of(array, val) >= 0;
	}
	
	int32_t snow_array_index_of(const SnObject* a, VALUE val) {
		const ArrayPrivate* priv = snow::object_get_private<ArrayPrivate>(a, SnArrayType);
		ASSERT(priv);
		return snow::index_of(*priv, val);
	}
}

namespace {
	VALUE array_initialize(const SnCallFrame* here, VALUE self, VALUE it) {
		ArrayPrivate* priv = snow::value_get_private<ArrayPrivate>(self, SnArrayType);
		if (priv == NULL) {
			snow_throw_exception_with_description("Array#initialize called for object that doesn't derive from Array.");
		}
		
		priv->reserve(here->args->size);
		priv->insert(priv->begin(), here->args->data, here->args->data + here->args->size);
		return self;
	}
	
	VALUE array_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
		ArrayPrivate* priv = snow::value_get_private<ArrayPrivate>(self, SnArrayType);
		if (priv == NULL) {
			snow_throw_exception_with_description("Array#inspect called for object that doesn't derive from Array.");
		}
		
		SnObject* result = snow_create_string_constant("@(");
		
		for (size_t i = 0; i < priv->size(); ++i) {
			VALUE val = (*priv)[i];
			snow_string_append(result, snow_value_inspect(val));
			if (i != priv->size() - 1) {
				snow_string_append_cstr(result, ", ");
			}
		}
		
		return result;
	}

	VALUE array_index_get(const SnCallFrame* here, VALUE self, VALUE it) {
		ArrayPrivate* priv = snow::value_get_private<ArrayPrivate>(self, SnArrayType);
		if (priv == NULL) return NULL;
		if (snow_type_of(it) != SnIntegerType) {
			snow_throw_exception_with_description("Array#get called with a non-integer index %p.", it);
		}
		return snow_array_get((SnObject*)self, snow_value_to_integer(it));
	}

	VALUE array_index_set(const SnCallFrame* here, VALUE self, VALUE it) {
		ArrayPrivate* priv = snow::value_get_private<ArrayPrivate>(self, SnArrayType);
		if (priv == NULL) return NULL;
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
		ArrayPrivate* priv = snow::value_get_private<ArrayPrivate>(self, SnArrayType);
		if (priv == NULL) return NULL;
		for (size_t i = 0; i < priv->size(); ++i) {
			VALUE args[] = { (*priv)[i] };
			snow_call(it, NULL, 1, args);
		}
		return SN_NIL;
	}

	VALUE array_push(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow::value_is_of_type(self, SnArrayType)) return NULL;
		// TODO: Check for recursion?
		snow_array_push((SnObject*)self, it);
		return self;
	}

	VALUE array_get_size(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow::value_is_of_type(self, SnArrayType)) return NULL;
		return snow_integer_to_value(snow_array_size((SnObject*)self));
	}
}

CAPI SnObject* snow_get_array_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class_for_type(snow_sym("Array"), &SnArrayType);
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
