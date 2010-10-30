#include "snow/array.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/type.h"

#include <string.h>

SnArray* snow_create_array() {
	SnArray* array = (SnArray*)snow_gc_alloc_object(SnArrayType);
	array->data = NULL;
	array->alloc_size = 0;
	array->size = 0;
	return array;
}

SnArray* snow_create_array_with_size(size_t sz) {
	SnArray* array = (SnArray*)snow_gc_alloc_object(SnArrayType);
	array->data = (VALUE*)malloc(sz * sizeof(VALUE));
	array->alloc_size = (uint32_t)sz;
	array->size = 0;
	return array;
}

SnArray* snow_create_array_from_range(VALUE* begin, VALUE* end) {
	size_t sz = end - begin;
	SnArray* array = snow_create_array_with_size(sz);
	array->size = sz;
	memcpy(array->data, begin, sz*sizeof(VALUE));
	return array;
}

size_t snow_array_size(const SnArray* array) {
	return array->size;
}

VALUE snow_array_get(const SnArray* array, int idx) {
	if (idx >= array->size)
		return NULL;
	if (idx < 0)
		idx += array->size;
	if (idx < 0)
		return NULL;
	// TODO: Lock?
	VALUE val = array->data[idx];
	return val;
}

VALUE snow_array_set(SnArray* array, int idx, VALUE val) {
	if (idx >= array->size) {
		const uint32_t new_size = idx + 1;
		snow_array_reserve(array, new_size);
		array->size = new_size;
	}
	if (idx < 0) {
		idx += array->size;
	}
	if (idx < 0) {
		TRAP(); // index out of bounds
		return NULL;
	}
	array->data[idx] = val;
	return val;
}

void snow_array_reserve(SnArray* array, uint32_t new_size) {
	if (new_size > array->alloc_size) {
		array->data = (VALUE*)realloc(array->data, new_size*sizeof(VALUE));
		memset(array->data + array->alloc_size, 0, new_size - array->alloc_size);
		array->alloc_size = new_size;
	}
}

SnArray* snow_array_push(SnArray* array, VALUE val) {
	snow_array_reserve(array, ++array->size);
	array->data[array->size-1] = val;
	return array;
}
