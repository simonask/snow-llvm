#include "intern.h"
#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/type.h"
#include "snow/function.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>

SnString* snow_create_string(const char* utf8) {
	return snow_create_string_with_size(utf8, strlen(utf8));
}

SnString* snow_create_string_constant(const char* utf8) {
	// TODO!
	return snow_create_string(utf8);
}

SnString* snow_create_string_with_size(const char* utf8, size_t size) {
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	obj->size = size;
	obj->length = size; // TODO: UTF-8 length
	obj->data = size ? (char*)malloc(size+1) : NULL;
	obj->constant = false;
	if (size) {
		memcpy(obj->data, utf8, size);
		obj->data[size] = '\0';
	}
	return obj;
}

SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf) {
	size_t s = snow_linkbuffer_size(buf);
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	obj->size = s;
	obj->length = s; // TODO: UTF-8 length
	obj->data = s ? (char*)malloc(s+1) : NULL;
	if (s) {
		snow_linkbuffer_copy_data(buf, obj->data, s);
		obj->data[s] = '\0';
	}
	return obj;
}

void snow_finalize_string(SnString* str) {
	free(str->data);
}

SnString* snow_string_concat(const SnString* a, const SnString* b) {
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	size_t size_a = snow_string_size(a);
	size_t size_b = snow_string_size(b);
	size_t s = size_a + size_b;
	obj->size = s;
	obj->length = s; // TODO: UTF-8 length
	obj->data = s ? (char*)malloc(s+1) : 0;
	memcpy(obj->data, snow_string_cstr(a), size_a);
	memcpy(obj->data + size_a, snow_string_cstr(b), size_b);
	obj->data[s] = '\0';
	return obj;
}

SnString* snow_string_format(const char* utf8_format, ...) {
	va_list ap;
	va_start(ap, utf8_format);
	char* str;
	vasprintf(&str, utf8_format, ap);
	va_end(ap);
	return snow_create_string(str);
}

static VALUE string_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnStringType);
	SnString* s = (SnString*)self;
	char* buffer = alloca(s->size + 3);
	memcpy(buffer+1, s->data, s->size);
	buffer[0] = '"';
	buffer[s->size + 1] = '"';
	buffer[s->size + 2] = '\0';
	return snow_create_string(buffer);
}

static VALUE string_to_string(SnFunctionCallContext* here, VALUE self, VALUE it) {
	return self;
}

SnObject* snow_create_string_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", string_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", string_to_string, 0);
	return proto;
}
