#include "intern.h"
#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/type.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>

typedef struct SnStringData {
	uint32_t size;
	uint32_t length; // number of UTF-8 characters
	char* data;
} SnStringData;

#define GET_DATA(VAR, OBJ) SnStringData* VAR = (SnStringData*)(OBJ)->data
#define GET_RDATA(VAR, REF) GET_DATA(VAR, (REF).obj)


static void string_initialize(SN_P p, SnObject* obj) {
	GET_DATA(data, obj);
	data->size = 0;
	data->length = 0;
	data->data = NULL;
}

static void string_finalize(SN_P p, SnObject* obj) {
	GET_DATA(data, obj);
	free(data->data);
}

static void string_copy(SN_P p, SnObject* copy, const SnObject* original) {
	GET_DATA(a, copy);
	GET_DATA(b, original);
	free(a->data);
	a->size = b->size;
	a->length = b->length;
	a->data = (char*)malloc(a->size + 1);
	memcpy(a->data, b->data, a->size);
	a->data[a->size] = '\0';
}

static void string_for_each_root(SN_P p, VALUE object, SnForEachRootCallbackFunc func, void* userdata) {
	// No roots in strings
}

static VALUE string_get_member(SN_P p, VALUE self, SnSymbol member) {
	return NULL;
}

static VALUE string_set_member(SN_P p, VALUE self, SnSymbol member, VALUE val) {
	return NULL;
}

static VALUE string_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	return functor;
}


static SnType _StringType;

const SnType* snow_get_string_type() {
	static const SnType* _type = NULL;
	if (!_type) {
		SnType* type = &_StringType;
		type->size = sizeof(SnStringData);
		type->initialize = string_initialize;
		type->finalize = string_finalize;
		type->copy = string_copy;
		type->for_each_root = string_for_each_root;
		type->get_member = string_get_member;
		type->set_member = string_set_member;
		type->call = string_call;
		_type = type;
	}
	return _type;
}


SnStringRef snow_create_string(SN_P p, const char* utf8) {
	return snow_create_string_with_size(p, utf8, strlen(utf8));
}

SnStringRef snow_create_string_with_size(SN_P p, const char* utf8, size_t size) {
	SnObject* obj = snow_gc_create_object(p, snow_get_string_type());
	GET_DATA(data, obj);
	data->size = size;
	data->length = size;
	data->data = size ? (char*)malloc(size+1) : NULL;
	memcpy(data->data, utf8, size);
	data->data[size] = '\0';
	return snow_object_as_string(obj);
}

SnStringRef snow_create_string_from_linkbuffer(SN_P p, struct SnLinkBuffer* buf) {
	size_t s = snow_linkbuffer_size(buf);
	SnObject* obj = snow_gc_create_object(p, snow_get_string_type());
	GET_DATA(data, obj);
	data->size = s;
	data->length = s;
	data->data = s ? (char*)malloc(s+1) : NULL;
	snow_linkbuffer_copy_data(buf, data->data, s);
	data->data[s] = '\0';
	return snow_object_as_string(obj);
}

SnStringRef snow_format_string(SN_P, const char* fmt, ...);

const char* snow_string_cstr(const SnStringRef str) {
	GET_RDATA(data, str);
	return data->data;
}

size_t snow_string_size(const SnStringRef str) {
	GET_RDATA(data, str);
	return data->size;
}

size_t snow_string_length(const SnStringRef str) {
	GET_RDATA(data, str);
	return data->length;
}

SnStringRef snow_string_concat(SN_P p, const SnStringRef a, const SnStringRef b) {
	SnObject* obj = snow_gc_create_object(p, snow_get_string_type());
	GET_DATA(data, obj);
	size_t size_a = snow_string_size(a);
	size_t size_b = snow_string_size(b);
	size_t s = size_a + size_b;
	data->size = s;
	data->length = s;
	data->data = s ? (char*)malloc(s+1) : 0;
	memcpy(data->data, snow_string_cstr(a), size_a);
	memcpy(data->data + size_a, snow_string_cstr(b), size_b);
	data->data[s] = '\0';
	return snow_object_as_string(obj);
}

bool snow_object_is_string(const SnObject* obj) {
	return obj->type == snow_get_string_type();
}

SnStringRef snow_object_as_string(SnObject* obj) {
	ASSERT(snow_object_is_string(obj));
	SnStringRef str;
	str.obj = obj;
	return str;
}