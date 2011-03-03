#include "intern.h"
#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/type.h"
#include "snow/function.h"
#include "snow/snow.h"
#include "snow/numeric.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>

SnString* snow_create_string(const char* utf8) {
	return snow_create_string_with_size(utf8, strlen(utf8));
}

SnString* snow_create_string_constant(const char* utf8) {
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	SN_GC_WRLOCK(obj);
	obj->size = strlen(utf8);
	obj->length = obj->size;
	obj->data = (char*)utf8;
	obj->constant = true;
	SN_GC_UNLOCK(obj);
	return obj;
}

SnString* snow_create_string_with_size(const char* utf8, size_t size) {
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	SN_GC_WRLOCK(obj);
	obj->size = size;
	obj->length = size; // TODO: UTF-8 length
	obj->data = size ? (char*)malloc(size+1) : NULL;
	obj->constant = false;
	if (size) {
		memcpy(obj->data, utf8, size);
		obj->data[size] = '\0';
	}
	SN_GC_UNLOCK(obj);
	return obj;
}

SnString* snow_create_string_take_ownership(char* utf8) {
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	SN_GC_WRLOCK(obj);
	obj->size = strlen(utf8);
	obj->length = obj->size;
	obj->data = utf8;
	obj->constant = false;
	SN_GC_UNLOCK(obj);
	return obj;
}

SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf) {
	size_t s = snow_linkbuffer_size(buf);
	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	SN_GC_WRLOCK(obj);
	obj->size = s;
	obj->length = s; // TODO: UTF-8 length
	obj->data = s ? (char*)malloc(s+1) : NULL;
	obj->constant = false;
	if (s) {
		snow_linkbuffer_copy_data(buf, obj->data, s);
		obj->data[s] = '\0';
	}
	SN_GC_UNLOCK(obj);
	return obj;
}

void snow_finalize_string(SnString* str) {
	if (!str->constant)
		free(str->data);
}

SnString* snow_string_concat(const SnString* a, const SnString* b) {
	size_t size_a = snow_string_size(a);
	size_t size_b = snow_string_size(b);
	size_t s = size_a + size_b;
	char* buffer = s ? (char*)malloc(s+1) : NULL;
	size_a = snow_string_copy_to(a, buffer, size_a);
	size_b = snow_string_copy_to(b, buffer + size_a, size_b);
	s = size_a + size_b;
	buffer[s] = '\0';

	SnString* obj = SN_GC_ALLOC_OBJECT(SnString);
	SN_GC_WRLOCK(obj);
	obj->size = s;
	obj->length = s; // TODO: UTF-8 length
	obj->data = buffer;
	SN_GC_UNLOCK(obj);
	return obj;
}

void snow_string_append(SnString* self, const SnString* other) {
	size_t other_size = snow_string_size(other);
	char* tmp = (char*)alloca(other_size);
	other_size = snow_string_copy_to(other, tmp, other_size);
	
	SN_GC_WRLOCK(self);
	const size_t combined_size = self->size + other_size;
	if (self->constant) {
		char* data = (char*)malloc(combined_size + 1);
		memcpy(data, self->data, self->size);
		self->data = data;
		self->constant = false;
	} else {
		self->data = (char*)realloc(self->data, combined_size + 1);
	}
	
	memcpy(self->data + self->size, tmp, other_size);
	self->data[combined_size] = '\0';
	self->size = combined_size;
	self->length = combined_size; // TODO: UTF-8 length
	SN_GC_UNLOCK(self);
}

void snow_string_append_cstr(SnString* self, const char* utf8) {
	size_t s = strlen(utf8);
	
	SN_GC_WRLOCK(self);
	size_t combined_size = self->size + s;
	if (self->constant) {
		char* data = (char*)malloc(combined_size + 1);
		memcpy(data, self->data, self->size);
		self->data = data;
		self->constant = false;
	} else {
		self->data = (char*)realloc(self->data, combined_size + 1);
	}
	
	memcpy(self->data + self->size, utf8, s);
	self->data[combined_size] = '\0';
	self->size = combined_size;
	self->length = combined_size; // TODO: UTF-8 length
	SN_GC_UNLOCK(self);
}

size_t snow_string_copy_to(const SnString* self, char* buffer, size_t max) {
	SN_GC_RDLOCK(self);
	size_t to_copy = self->size < max ? self->size : max;
	memcpy(buffer, self->data, to_copy);
	SN_GC_UNLOCK(self);
	return to_copy;
}

SnString* snow_string_format(const char* utf8_format, ...) {
	va_list ap;
	va_start(ap, utf8_format);
	char* str;
	vasprintf(&str, utf8_format, ap);
	va_end(ap);
	return snow_create_string_take_ownership(str);
}

size_t snow_string_size(const SnString* str) {
	SN_GC_RDLOCK(str);
	size_t sz = str->size;
	SN_GC_UNLOCK(str);
	return sz;
}

size_t snow_string_length(const SnString* str) {
	SN_GC_RDLOCK(str);
	size_t len = str->length;
	SN_GC_UNLOCK(str);
	return len;
}

static VALUE string_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnStringType);
	SnString* s = (SnString*)self;
	size_t size = snow_string_size(s);
	char* buffer = (char*)alloca(size + 3);
	size = snow_string_copy_to(s, buffer+1, size);
	buffer[0] = '"';
	buffer[size + 1] = '"';
	buffer[size + 2] = '\0';
	return snow_create_string_with_size(buffer, size + 3);
}

static VALUE string_to_string(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnStringType);
	return self;
}

static VALUE string_add(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnStringType) return NULL;
	
	if (it) {
		SnString* other = snow_value_to_string(it);
		return snow_string_concat((SnString*)self, other);
	}
	return self;
}

static VALUE string_get_size(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) == SnStringType) {
		return snow_integer_to_value((int64_t)snow_string_size((SnString*)self));
	}
	return NULL;
}

static VALUE string_get_length(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) == SnStringType) {
		return snow_integer_to_value((int64_t)snow_string_length((SnString*)self));
	}
	return NULL;
}

SnObject* snow_create_string_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", string_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", string_to_string, 0);
	SN_DEFINE_METHOD(proto, "+", string_add, 1);
	SN_DEFINE_PROPERTY(proto, "size", string_get_size, NULL);
	SN_DEFINE_PROPERTY(proto, "length", string_get_length, NULL);
	return proto;
}
