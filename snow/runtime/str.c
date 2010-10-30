#include "intern.h"
#include "snow/str.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/type.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>

SnString* snow_create_string(const char* utf8) {
	return snow_create_string_with_size(utf8, strlen(utf8));
}

SnString* snow_create_string_with_size(const char* utf8, size_t size) {
	SnString* obj = (SnString*)snow_gc_alloc_object(SnStringType);
	obj->size = size;
	obj->length = size; // TODO: UTF-8 length
	obj->data = size ? (char*)malloc(size+1) : NULL;
	memcpy(obj->data, utf8, size);
	obj->data[size] = '\0';
	return obj;
}

SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf) {
	size_t s = snow_linkbuffer_size(buf);
	SnString* obj = (SnString*)snow_gc_alloc_object(SnStringType);
	obj->size = s;
	obj->length = s; // TODO: UTF-8 length
	obj->data = s ? (char*)malloc(s+1) : NULL;
	snow_linkbuffer_copy_data(buf, obj->data, s);
	obj->data[s] = '\0';
	return obj;
}

SnString* snow_string_concat(const SnString* a, const SnString* b) {
	SnString* obj = (SnString*)snow_gc_alloc_object(SnStringType);
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
