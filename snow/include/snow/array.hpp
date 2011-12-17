#pragma once
#ifndef ARRAY_H_XVGE8JQI
#define ARRAY_H_XVGE8JQI

#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Array;
	typedef const ObjectPtr<Array>& ArrayPtr;
	typedef const ObjectPtr<const Array>& ArrayConstPtr;
	
	ObjectPtr<Array> create_array();
	ObjectPtr<Array> create_array_from_range(VALUE* begin, VALUE* end);
	ObjectPtr<Array> create_array_with_size(uint32_t alloc_size);
	
	size_t array_size(ArrayConstPtr array);
	VALUE array_get(ArrayConstPtr array, int32_t idx);
	VALUE array_set(ArrayPtr array, int32_t idx, VALUE val);
	size_t array_get_all(ArrayConstPtr array, VALUE* out_values, size_t max);
	void array_reserve(ArrayPtr array, uint32_t new_alloc_size);
	void array_push(ArrayPtr array, VALUE val);
	void array_push_range(ArrayPtr array, VALUE* begin, VALUE* end);
	ObjectPtr<Array> array_concatenate(ArrayConstPtr a, ArrayConstPtr b);
	bool array_contains(ArrayConstPtr a, VALUE val);
	int32_t array_index_of(ArrayConstPtr a, VALUE val);
	
	SnObject* get_array_class();
}

#endif /* end of include guard: ARRAY_H_XVGE8JQI */
