#include "snow/array.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/type.h"
#include "snow/str.h"
#include "snow/function.h"
#include "snow/numeric.h"

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


static VALUE array_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnArrayType);
	SnArray* array = (SnArray*)self;
	
	SnString** inspected = (SnString**)alloca(array->size*sizeof(SnString*));
	size_t complete_size = 0;
	for (size_t i = 0; i < array->size; ++i) {
		VALUE x = array->data[i];
		inspected[i] = (SnString*)snow_call(snow_get_method(x, snow_sym("inspect")), x, 0, NULL);
		ASSERT(snow_type_of(inspected[i]) == SnStringType);
		complete_size += inspected[i]->size;
	}
	
	char* str = malloc(complete_size+4);
	str[0] = '@';
	str[1] = '(';
	char* p = str + 2;
	for (size_t i = 0; i < array->size; ++i) {
		memcpy(p, inspected[i]->data, inspected[i]->size);
		p += inspected[i]->size;
	}
	*p = ')';
	++p;
	*p = '\0';
	
	return snow_create_string(str);
}

static VALUE array_index_get(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnArrayType);
	ASSERT(snow_type_of(it) == SnIntegerType);
	return snow_array_get((SnArray*)self, snow_value_to_integer(it));
}

static VALUE array_index_set(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnArrayType);
	ASSERT(snow_type_of(it) == SnIntegerType);
	VALUE val = here->locals[1]; // second arg
	return snow_array_set((SnArray*)self, snow_value_to_integer(it), val);
}

SnObject* snow_create_array_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", array_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", array_inspect, 0);
	SN_DEFINE_METHOD(proto, "__index_get__", array_index_get, 1);
	SN_DEFINE_METHOD(proto, "__index_set__", array_index_set, 2);
	return proto;
}