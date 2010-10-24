#include "snow/array.h"
#include "snow/object.h"
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/type.h"

#include <string.h>

typedef struct SnArrayData {
	VALUE* data;
	uint32_t alloc_size;
	uint32_t size;
} SnArrayData;

#define GET_DATA(VAR, OBJ) SnArrayData* VAR = (SnArrayData*)(OBJ)->data
#define GET_RDATA(VAR, REF) GET_DATA(VAR, (REF).obj)

static void array_initialize(SN_P p, SnObject* obj) {}
static void array_finalize(SN_P p, SnObject* obj) {}
static void array_copy(SN_P p, SnObject* copy, const SnObject* original) {}
static void array_for_each_root(SN_P p, VALUE object, SnForEachRootCallbackFunc func, void* userdata) {}
static VALUE array_get_member(SN_P p, VALUE object, SnSymbol member) { return NULL; }
static VALUE array_set_member(SN_P p, VALUE object, SnSymbol member, VALUE val) { return NULL; }
static VALUE array_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) { return NULL; }

static SnType _ArrayType;
const SnType* snow_get_array_type() {
	static const SnType* _type = NULL;
	if (!_type) {
		SnType* type = &_ArrayType;
		type->size = sizeof(SnArrayData);
		type->initialize = array_initialize;
		type->finalize = array_finalize;
		type->copy = array_copy;
		type->for_each_root = array_for_each_root;
		type->get_member = array_get_member;
		type->set_member = array_set_member;
		type->call = array_call;
		_type = type;
	}
	return _type;
}

SnArrayRef snow_create_array(SN_P p) {
	SnObject* obj = snow_gc_create_object(p, snow_get_array_type());
	GET_DATA(data, obj);
	data->data = NULL;
	data->alloc_size = 0;
	data->size = 0;
	return snow_object_as_array(obj);
}

size_t snow_array_size(SnArrayRef array) {
	GET_RDATA(data, array);
	return data->size;
}

VALUE snow_array_get(SnArrayRef array, int idx) {
	GET_RDATA(data, array);
	if (idx >= data->size)
		return NULL;
	if (idx < 0)
		idx += data->size;
	if (idx < 0)
		return NULL;
	// TODO: Lock?
	VALUE val = data->data[idx];
	return val;
}

VALUE snow_array_set(SN_P p, SnArrayRef array, int idx, VALUE val) {
	GET_RDATA(data, array);
	snow_modify(p, array.obj);
	snow_array_reserve(p, array, idx);
	if (idx >= data->size) {
		data->size = idx + 1;
	}
	if (idx < 0) {
		idx += data->size;
	}
	if (idx < 0) {
		TRAP(); // index out of bounds
		return NULL;
	}
	data->data[idx] = val;
	return val;
}

void snow_array_reserve(SN_P p, SnArrayRef array, uint32_t new_size) {
	GET_RDATA(data, array);
	snow_modify(p, array.obj);
	if (new_size > data->alloc_size) {
		data->data = (VALUE*)realloc(data->data, new_size*sizeof(VALUE));
		memset(data->data + data->alloc_size, 0, new_size - data->alloc_size);
		data->alloc_size = new_size;
	}
}

SnArrayRef snow_array_push(SN_P p, SnArrayRef array, VALUE val) {
	GET_RDATA(data, array);
	snow_array_reserve(p, array, ++data->size);
	data->data[data->size-1] = val;
	return array;
}

SnArrayRef snow_object_as_array(SnObject* obj) {
	ASSERT(obj->type == snow_get_array_type());
	SnArrayRef array;
	array.obj = obj;
	return array;
}