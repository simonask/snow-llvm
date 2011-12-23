#pragma once
#ifndef ARRAY_H_XVGE8JQI
#define ARRAY_H_XVGE8JQI

#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Array;
	struct Class;
	typedef ObjectPtr<Array> ArrayPtr;
	typedef ObjectPtr<const Array> ArrayConstPtr;
	
	ObjectPtr<Array> create_array();
	ObjectPtr<Array> create_array_from_range(const Value* begin, const Value* end);
	ObjectPtr<Array> create_array_with_size(uint32_t alloc_size);
	
	size_t array_size(ArrayConstPtr array);
	Value array_get(ArrayConstPtr array, int32_t idx);
	Value& array_set(ArrayPtr array, int32_t idx, Value val);
	size_t array_get_all(ArrayConstPtr array, Value* out_values, size_t max);
	void array_reserve(ArrayPtr array, uint32_t new_alloc_size);
	void array_push(ArrayPtr array, Value val);
	void array_push_range(ArrayPtr array, const Value* begin, const Value* end);
	ObjectPtr<Array> array_concatenate(ArrayConstPtr a, ArrayConstPtr b);
	bool array_contains(ArrayConstPtr a, Value val);
	int32_t array_index_of(ArrayConstPtr a, Value val);
	
	ObjectPtr<Class> get_array_class();
}

#endif /* end of include guard: ARRAY_H_XVGE8JQI */
