#include "snow/array.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/type.h"
#include "snow/str.h"
#include "snow/function.h"
#include "snow/numeric.h"
#include "prefetch.h"

#include <string.h>

SnArray* snow_create_array() {
	SnArray* array = SN_GC_ALLOC_OBJECT(SnArray);
	SN_GC_WRLOCK(array);
	array->data = NULL;
	array->alloc_size = 0;
	array->size = 0;
	SN_GC_UNLOCK(array);
	return array;
}

SnArray* snow_create_array_with_size(size_t sz) {
	SnArray* array = SN_GC_ALLOC_OBJECT(SnArray);
	SN_GC_WRLOCK(array);
	array->data = (VALUE*)malloc(sz * sizeof(VALUE));
	array->alloc_size = (uint32_t)sz;
	array->size = 0;
	SN_GC_UNLOCK(array);
	return array;
}

SnArray* snow_create_array_from_range(VALUE* begin, VALUE* end) {
	size_t sz = end - begin;
	SnArray* array = snow_create_array_with_size(sz);
	SN_GC_WRLOCK(array);
	array->size = sz;
	memcpy(array->data, begin, sz*sizeof(VALUE));
	SN_GC_UNLOCK(array);
	return array;
}

void snow_finalize_array(SnArray* a) {
	free(a->data);
}

size_t snow_array_size(const SnArray* array) {
	SN_GC_RDLOCK(array);
	size_t size = array->size;
	SN_GC_UNLOCK(array);
	return size;
}

static inline size_t array_get_size_and_prefetch(const SnArray* array) {
	SN_GC_RDLOCK(array);
	size_t sz = array->size;
	snow_prefetch(array->data, SnPrefetchRead, SnPrefetchLocalityMedium);
	SN_GC_UNLOCK(array);
	return sz;
}

VALUE snow_array_get(const SnArray* array, int idx) {
	SN_GC_RDLOCK(array);
	VALUE val;
	if (idx >= (int32_t)array->size) {
		val = NULL;
		goto out;
	}
	if (idx < 0)
		idx += array->size;
	if (idx < 0) {
		val = NULL;
		goto out;
	}
	val = array->data[idx];
out:
	SN_GC_UNLOCK(array);
	return val;
}

size_t snow_array_get_all(const SnArray* array, VALUE* out_values, size_t max) {
	SN_GC_RDLOCK(array);
	snow_prefetch(array->data, SnPrefetchRead, SnPrefetchLocalityLow);
	size_t to_copy = array->size < max ? array->size : max;
	memcpy(out_values, array->data, sizeof(VALUE)*max);
	SN_GC_UNLOCK(array);
	return to_copy;
}

VALUE snow_array_set(SnArray* array, int idx, VALUE val) {
	SN_GC_WRLOCK(array);
	
	if (idx >= (int32_t)array->size) {
		const uint32_t new_size = idx + 1;
		snow_array_reserve(array, new_size);
		array->size = new_size;
	}
	if (idx < 0) {
		idx += array->size;
	}
	if (idx < 0) {
		// TODO: EXCEPTION.
		TRAP(); // index out of bounds
		goto out;
	}
	array->data[idx] = val;
out:
	SN_GC_UNLOCK(array);
	return val;
}

static void snow_array_reserve_unlocked(SnArray* array, uint32_t new_size) {
	if (new_size > array->alloc_size) {
		array->data = (VALUE*)realloc(array->data, new_size*sizeof(VALUE));
		memset(array->data + array->alloc_size, 0, new_size - array->alloc_size);
		array->alloc_size = new_size;
	}
}

void snow_array_reserve(SnArray* array, uint32_t new_size) {
	SN_GC_WRLOCK(array);
	snow_array_reserve_unlocked(array, new_size);
	SN_GC_UNLOCK(array);
}

SnArray* snow_array_push(SnArray* array, VALUE val) {
	SN_GC_WRLOCK(array);
	snow_array_reserve_unlocked(array, ++array->size);
	array->data[array->size-1] = val;
	SN_GC_UNLOCK(array);
	return array;
}

bool snow_array_contains(SnArray* array, VALUE val) {
	SN_GC_RDLOCK(array);
	snow_prefetch(array->data, SnPrefetchRead, SnPrefetchLocalityLow);
	bool found = false;
	for (size_t i = 0; i < array->size; ++i) {
		if (array->data[i] == val) {
			found = true;
			break;
		}
	}
	SN_GC_UNLOCK(array);
	return found;
}


static VALUE array_inspect(SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnArrayType);
	SnArray* array = (SnArray*)self;
	
	SN_GC_RDLOCK(array);
	SnString** inspected = (SnString**)alloca(array->size*sizeof(SnString*));
	size_t complete_size = 0;
	for (size_t i = 0; i < array->size; ++i) {
		VALUE val = array->data[i];
		SN_GC_UNLOCK(array); // must release lock while calling .inspect
		inspected[i] = snow_value_inspect(val);
		SN_GC_RDLOCK(array);
		complete_size += inspected[i]->size;
	}
	
	complete_size += array->size ? (array->size-1) * 2 : 0;
	
	char* str = malloc(complete_size + 4);
	str[0] = '@';
	str[1] = '(';
	char* p = str + 2;
	for (size_t i = 0; i < array->size; ++i) {
		memcpy(p, inspected[i]->data, inspected[i]->size);
		p += inspected[i]->size;
		if (i != array->size - 1) {
			p[0] = ',';
			p[1] = ' ';
			p += 2;
		}
	}
	*p = ')';
	++p;
	*p = '\0';
	SN_GC_UNLOCK(array);
	
	return snow_create_string_take_ownership(str);
}

static VALUE array_index_get(SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnArrayType);
	ASSERT(snow_type_of(it) == SnIntegerType);
	return snow_array_get((SnArray*)self, snow_value_to_integer(it));
}

static VALUE array_index_set(SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnArrayType);
	ASSERT(snow_type_of(it) == SnIntegerType);
	VALUE val = snow_arguments_get_by_index(here->arguments, 1); // second arg
	return snow_array_set((SnArray*)self, snow_value_to_integer(it), val);
}

static VALUE array_multiply_or_splat(SnCallFrame* here, VALUE self, VALUE it) {
	if (!it) return self;
	return self; // TODO: Something useful?
}

static VALUE array_each(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnArrayType) return NULL;
	SnArray* array = (SnArray*)self;
	size_t sz = array_get_size_and_prefetch(array);
	for (size_t i = 0; i < sz; ++i) {
		VALUE args[] = { snow_array_get(array, i), snow_integer_to_value(i) };
		snow_call(it, NULL, 2, args);
	}
	return SN_NIL;
}

static VALUE array_push(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnArrayType) return NULL;
	SnArray* array = (SnArray*)self;
	snow_array_push(array, it);
	return self;
}

SnObject* snow_create_array_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", array_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", array_inspect, 0);
	SN_DEFINE_METHOD(proto, "get", array_index_get, 1);
	SN_DEFINE_METHOD(proto, "set", array_index_set, 2);
	SN_DEFINE_METHOD(proto, "*", array_multiply_or_splat, 1);
	SN_DEFINE_METHOD(proto, "each", array_each, 1);
	SN_DEFINE_METHOD(proto, "push", array_push, 1);
	SN_DEFINE_METHOD(proto, "<<", array_push, 1);
	return proto;
}