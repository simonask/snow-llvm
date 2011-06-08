#include "snow/str.h"
#include "internal.h"
#include "snow/class.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"
#include "snow/numeric.h"
#include "snow/snow.h"
#include "snow/type.h"
#include "snow/exception.h"

#include "util.hpp"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>

namespace {
	struct StringPrivate {
		char* data; // not NULL-terminated!
		uint32_t size;
		uint32_t length;
		bool constant;

		StringPrivate() : data(NULL), size(0), length(0), constant(true) {}
		~StringPrivate() {
			if (!constant)
				delete[] data;
		}
		StringPrivate& operator=(const StringPrivate& other) {
			constant = other.constant;
			size = other.size;
			length = other.length;
			if (!constant) {
				data = new char[size];
				snow::copy_range(data, other.data, size);
			}
			return *this;
		}
	};

	void string_gc_each_root(void* data, void(*callback)(VALUE*)) {
		// dummy -- strings have no references to other objects
	}
	
	uint32_t get_utf8_length(const char* utf8, uint32_t size) {
		// TODO: Can be optimized quite a bit, but consider using a library.
		uint32_t len = 0;
		for (size_t i = 0; i < size; ++i) {
			if ((utf8[i] & 0xc0) != 0x80)
				len++;
		}
		return len;
	}
	
	uint32_t get_utf8_length(const char* utf8) {
		return get_utf8_length(utf8, strlen(utf8));
	}
}

CAPI {
	SnObjectType SnStringType = {
		.data_size = sizeof(StringPrivate),
		.initialize = snow::construct<StringPrivate>,
		.finalize = snow::destruct<StringPrivate>,
		.copy = snow::assign<StringPrivate>,
		.gc_each_root = string_gc_each_root,
	};

	SnObject* snow_create_string(const char* utf8) {
		return snow_create_string_with_size(utf8, strlen(utf8));
	}

	SnObject* snow_create_string_constant(const char* utf8) {
		SnObject* str = snow_create_object(snow_get_string_class(), 0, NULL);
		StringPrivate* priv = snow::object_get_private<StringPrivate>(str, SnStringType);
		ASSERT(priv);
		priv->size = strlen(utf8);
		priv->data = (char*)utf8;
		priv->constant = true;
		priv->length = get_utf8_length(utf8, priv->size);
		return str;
	}

	SnObject* snow_create_string_with_size(const char* utf8, size_t size) {
		SnObject* str = snow_create_object(snow_get_string_class(), 0, NULL);
		StringPrivate* priv = snow::object_get_private<StringPrivate>(str, SnStringType);
		ASSERT(priv);
		priv->size = size;
		if (size) {
			priv->data = new char[size];
			snow::copy_range(priv->data, utf8, size);
			priv->length = get_utf8_length(utf8, size);
			priv->constant = false;
		}
		return str;
	}

	SnObject* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf) {
		size_t s = snow_linkbuffer_size(buf);
		SnObject* str = snow_create_object(snow_get_string_class(), 0, NULL);
		StringPrivate* priv = snow::object_get_private<StringPrivate>(str, SnStringType);
		ASSERT(priv);
		priv->size = s;
		if (s) {
			priv->data = new char[s];
			snow_linkbuffer_copy_data(buf, priv->data, s);
			priv->length = get_utf8_length(priv->data, s);
			priv->constant = false;
		}
		return str;
	}
	
	bool snow_is_string(VALUE val) {
		return snow::value_is_of_type(val, SnStringType);
	}

	SnObject* snow_string_concat(const SnObject* a, const SnObject* b) {
		size_t size_a = snow_string_size(a);
		size_t size_b = snow_string_size(b);
		size_t s = size_a + size_b;
		char* buffer = s ? new char(s) : NULL;
		size_a = snow_string_copy_to(a, buffer, size_a);
		size_b = snow_string_copy_to(b, buffer + size_a, size_b);
		s = size_a + size_b;
		
		SnObject* str = snow_create_object(snow_get_string_class(), 0, NULL);
		StringPrivate* priv = snow::object_get_private<StringPrivate>(str, SnStringType);
		ASSERT(priv);
		priv->size = s;
		priv->data = buffer;
		priv->length = get_utf8_length(buffer, s);
		priv->constant = false;
		return str;
	}

	void snow_string_append(SnObject* self, const SnObject* other) {
		size_t other_size = snow_string_size(other);
		char tmp[other_size];
		other_size = snow_string_copy_to(other, tmp, other_size);

		SN_GC_WRLOCK(self);
		StringPrivate* priv = snow::object_get_private<StringPrivate>(self, SnStringType);
		const size_t combined_size = priv->size + other_size;
		if (priv->constant) {
			char* data = new char[combined_size];
			snow::copy_range(data, priv->data, priv->size);
			priv->data = data;
			priv->constant = false;
		} else {
			priv->data = snow::realloc_range(priv->data, priv->size, combined_size);
		}

		snow::copy_range(priv->data + priv->size, tmp, other_size);
		priv->size = combined_size;
		priv->length = get_utf8_length(priv->data, combined_size);
		SN_GC_UNLOCK(self);
	}

	void snow_string_append_cstr(SnObject* self, const char* utf8) {
		size_t s = strlen(utf8);

		SN_GC_WRLOCK(self);
		StringPrivate* priv = snow::object_get_private<StringPrivate>(self, SnStringType);
		size_t combined_size = priv->size + s;
		if (priv->constant) {
			char* data = new char[combined_size];
			snow::copy_range(data, priv->data, priv->size);
			priv->data = data;
			priv->constant = false;
		} else {
			priv->data = snow::realloc_range(priv->data, priv->size, combined_size);
		}

		snow::copy_range(priv->data + priv->size, utf8, s);
		priv->size = combined_size;
		priv->length = get_utf8_length(priv->data, combined_size);
		SN_GC_UNLOCK(self);
	}

	size_t snow_string_copy_to(const SnObject* self, char* buffer, size_t max) {
		SN_GC_RDLOCK(self);
		const StringPrivate* priv = snow::object_get_private<StringPrivate>(self, SnStringType);
		size_t to_copy = priv->size < max ? priv->size : max;
		snow::copy_range(buffer, priv->data, to_copy);
		SN_GC_UNLOCK(self);
		return to_copy;
	}

	SnObject* snow_string_format(const char* utf8_format, ...) {
		va_list ap;
		va_start(ap, utf8_format);
		SnObject* str = snow_string_format_va(utf8_format, ap);
		va_end(ap);
		return str;
	}

	SnObject* snow_string_format_va(const char* utf8_format, va_list ap) {
		char* str;
		vasprintf(&str, utf8_format, ap);
		SnObject* obj = snow_create_string(str);
		free(str);
		return obj;
	}

	size_t snow_string_size(const SnObject* str) {
		SN_GC_RDLOCK(str);
		const StringPrivate* priv = snow::object_get_private<StringPrivate>(str, SnStringType);
		ASSERT(priv);
		size_t sz = priv->size;
		SN_GC_UNLOCK(str);
		return sz;
	}

	size_t snow_string_length(const SnObject* str) {
		SN_GC_RDLOCK(str);
		const StringPrivate* priv = snow::object_get_private<StringPrivate>(str, SnStringType);
		ASSERT(priv);
		size_t length = priv->length;
		SN_GC_UNLOCK(str);
		return length;
	}

	static VALUE string_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow_is_string(self)) snow_throw_exception_with_description("String#inspect called on something that's not a string: %p.", self);
		SnObject* s = (SnObject*)self;
		size_t size = snow_string_size(s);
		char buffer[size + 2];
		size = snow_string_copy_to(s, buffer+1, size);
		buffer[0] = '"';
		buffer[size + 1] = '"';
		return snow_create_string_with_size(buffer, size + 2);
	}

	static VALUE string_to_string(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow_is_string(self)) snow_throw_exception_with_description("String#to_string called on something that's not a string: %p.", self);
		return self;
	}

	static VALUE string_add(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow_is_string(self)) snow_throw_exception_with_description("String#+ called on something that's not a string: %p.", self);

		if (it) {
			SnObject* other = snow_value_to_string(it);
			return snow_string_concat((SnObject*)self, other);
		}
		return self;
	}

	static VALUE string_get_size(const SnCallFrame* here, VALUE self, VALUE it) {
		if (snow_is_string(self)) {
			return snow_integer_to_value((int64_t)snow_string_size((SnObject*)self));
		}
		return NULL;
	}

	static VALUE string_get_length(const SnCallFrame* here, VALUE self, VALUE it) {
		if (snow_is_string(self)) {
			return snow_integer_to_value((int64_t)snow_string_length((SnObject*)self));
		}
		return NULL;
	}

	SnObject* snow_get_string_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = snow_create_class_for_type(snow_sym("String"), &SnStringType);
			snow_class_define_method(cls, "inspect", string_inspect);
			snow_class_define_method(cls, "to_string", string_to_string);
			snow_class_define_method(cls, "+", string_add);
			snow_class_define_property(cls, "size", string_get_size, NULL);
			snow_class_define_property(cls, "length", string_get_length, NULL);
			root = snow_gc_create_root(cls);
		}
		return *root;
	}
}
