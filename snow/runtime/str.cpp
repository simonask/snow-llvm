#include "snow/str.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "snow/linkbuffer.h"
#include "snow/numeric.hpp"
#include "snow/snow.hpp"
#include "snow/exception.h"

#include "snow/util.hpp"
#include "snow/objectptr.hpp"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>

namespace snow {
	struct String {
		char* data; // not NULL-terminated!
		uint32_t size;
		uint32_t length;
		bool constant;

		String() : data(NULL), size(0), length(0), constant(true) {}
		~String() {
			if (!constant)
				delete[] data;
		}
		String& operator=(const String& other) {
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

SN_REGISTER_SIMPLE_CPP_TYPE(String)

CAPI {
	using namespace snow;

	SnObject* snow_create_string(const char* utf8) {
		return snow_create_string_with_size(utf8, strlen(utf8));
	}

	SnObject* snow_create_string_constant(const char* utf8) {
		ObjectPtr<String> str = snow_create_object(snow_get_string_class(), 0, NULL);
		str->size = strlen(utf8);
		str->data = (char*)utf8;
		str->constant = true;
		str->length = get_utf8_length(utf8, str->size);
		return str;
	}

	SnObject* snow_create_string_with_size(const char* utf8, size_t size) {
		ObjectPtr<String> str = snow_create_object(snow_get_string_class(), 0, NULL);
		str->size = size;
		if (size) {
			str->data = new char[size];
			snow::copy_range(str->data, utf8, size);
			str->length = get_utf8_length(utf8, size);
			str->constant = false;
		}
		return str;
	}

	SnObject* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf) {
		size_t s = snow_linkbuffer_size(buf);
		ObjectPtr<String> str = snow_create_object(snow_get_string_class(), 0, NULL);
		str->size = s;
		if (s) {
			str->data = new char[s];
			snow_linkbuffer_copy_data(buf, str->data, s);
			str->length = get_utf8_length(str->data, s);
			str->constant = false;
		}
		return str;
	}
	
	bool snow_is_string(VALUE val) {
		return snow::value_is_of_type(val, get_type<String>());
	}

	SnObject* snow_string_concat(const SnObject* a, const SnObject* b) {
		size_t size_a = snow_string_size(a);
		size_t size_b = snow_string_size(b);
		size_t s = size_a + size_b;
		char* buffer = s ? new char(s) : NULL;
		size_a = snow_string_copy_to(a, buffer, size_a);
		size_b = snow_string_copy_to(b, buffer + size_a, size_b);
		s = size_a + size_b;
		
		ObjectPtr<String> str = snow_create_object(snow_get_string_class(), 0, NULL);
		str->size = s;
		str->data = buffer;
		str->length = get_utf8_length(buffer, s);
		str->constant = false;
		return str;
	}

	void snow_string_append(SnObject* self, const SnObject* other) {
		size_t other_size = snow_string_size(other);
		char tmp[other_size];
		other_size = snow_string_copy_to(other, tmp, other_size);

		SN_GC_WRLOCK(self);
		ObjectPtr<String> str = self;
		const size_t combined_size = str->size + other_size;
		if (str->constant) {
			char* data = new char[combined_size];
			snow::copy_range(data, str->data, str->size);
			str->data = data;
			str->constant = false;
		} else {
			str->data = snow::realloc_range(str->data, str->size, combined_size);
		}

		snow::copy_range(str->data + str->size, tmp, other_size);
		str->size = combined_size;
		str->length = get_utf8_length(str->data, combined_size);
		SN_GC_UNLOCK(self);
	}

	void snow_string_append_cstr(SnObject* self, const char* utf8) {
		size_t s = strlen(utf8);

		SN_GC_WRLOCK(self);
		ObjectPtr<String> str = self;
		size_t combined_size = str->size + s;
		if (str->constant) {
			char* data = new char[combined_size];
			snow::copy_range(data, str->data, str->size);
			str->data = data;
			str->constant = false;
		} else {
			str->data = snow::realloc_range(str->data, str->size, combined_size);
		}

		snow::copy_range(str->data + str->size, utf8, s);
		str->size = combined_size;
		str->length = get_utf8_length(str->data, combined_size);
		SN_GC_UNLOCK(self);
	}

	size_t snow_string_copy_to(const SnObject* self, char* buffer, size_t max) {
		SN_GC_RDLOCK(self);
		ObjectPtr<const String> str = self;
		size_t to_copy = str->size < max ? str->size : max;
		snow::copy_range(buffer, str->data, to_copy);
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
		return ObjectPtr<const String>(str)->size;
	}

	size_t snow_string_length(const SnObject* str) {
		return ObjectPtr<const String>(str)->length;
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
			SnObject* cls = snow_create_class_for_type(snow_sym("String"), get_type<String>());
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
