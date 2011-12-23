#include "snow/str.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "linkbuffer.hpp"
#include "snow/numeric.hpp"
#include "snow/snow.hpp"
#include "snow/exception.hpp"

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
	
	SN_REGISTER_SIMPLE_CPP_TYPE(String)
	
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

	ObjectPtr<String> create_string(const char* utf8) {
		return create_string_with_size(utf8, strlen(utf8));
	}

	ObjectPtr<String> create_string_constant(const char* utf8) {
		ObjectPtr<String> str = create_object(get_string_class(), 0, NULL);
		str->size = strlen(utf8);
		str->data = (char*)utf8;
		str->constant = true;
		str->length = get_utf8_length(utf8, str->size);
		return str;
	}

	ObjectPtr<String> create_string_with_size(const char* utf8, size_t size) {
		ObjectPtr<String> str = create_object(get_string_class(), 0, NULL);
		str->size = size;
		if (size) {
			str->data = new char[size];
			snow::copy_range(str->data, utf8, size);
			str->length = get_utf8_length(utf8, size);
			str->constant = false;
		}
		return str;
	}

	ObjectPtr<String> create_string_from_linkbuffer(const LinkBuffer<char>& buf) {
		size_t s = buf.size();
		ObjectPtr<String> str = create_object(get_string_class(), 0, NULL);
		str->size = s;
		if (s) {
			str->data = new char[s];
			buf.extract(str->data, s);
			str->length = get_utf8_length(str->data, s);
			str->constant = false;
		}
		return str;
	}
	
	bool is_string(Value val) {
		return snow::value_is_of_type(val, get_type<String>());
	}

	ObjectPtr<String> string_concat(StringConstPtr a, StringConstPtr b) {
		size_t size_a = string_size(a);
		size_t size_b = string_size(b);
		size_t s = size_a + size_b;
		char* buffer = s ? new char(s) : NULL;
		size_a = string_copy_to(a, buffer, size_a);
		size_b = string_copy_to(b, buffer + size_a, size_b);
		s = size_a + size_b;
		
		ObjectPtr<String> str = create_object(get_string_class(), 0, NULL);
		str->size = s;
		str->data = buffer;
		str->length = get_utf8_length(buffer, s);
		str->constant = false;
		return str;
	}

	void string_append(StringPtr str, StringConstPtr other) {
		size_t other_size = string_size(other);
		char tmp[other_size];
		other_size = string_copy_to(other, tmp, other_size);

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
	}

	void string_append_cstr(StringPtr str, const char* utf8) {
		size_t s = strlen(utf8);

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
	}

	size_t string_copy_to(StringConstPtr str, char* buffer, size_t max) {
		size_t to_copy = str->size < max ? str->size : max;
		snow::copy_range(buffer, str->data, to_copy);
		return to_copy;
	}

	ObjectPtr<String> string_format(const char* utf8_format, ...) {
		va_list ap;
		va_start(ap, utf8_format);
		ObjectPtr<String> str = string_format_va(utf8_format, ap);
		va_end(ap);
		return str;
	}

	ObjectPtr<String> string_format_va(const char* utf8_format, va_list ap) {
		char* str;
		vasprintf(&str, utf8_format, ap);
		ObjectPtr<String> obj = create_string(str);
		free(str);
		return obj;
	}

	size_t string_size(StringConstPtr str) {
		return str->size;
	}

	size_t string_length(StringConstPtr str) {
		return str->length;
	}

	namespace bindings {
		static VALUE string_inspect(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_string(self)) throw_exception_with_description("String#inspect called on something that's not a string: %p.", self);
			ObjectPtr<String> s = self;
			size_t size = string_size(s);
			char buffer[size + 2];
			size = string_copy_to(s, buffer+1, size);
			buffer[0] = '"';
			buffer[size + 1] = '"';
			return create_string_with_size(buffer, size + 2);
		}

		static VALUE string_to_string(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_string(self)) throw_exception_with_description("String#to_string called on something that's not a string: %p.", self);
			return self;
		}

		static VALUE string_add(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_string(self)) throw_exception_with_description("String#+ called on something that's not a string: %p.", self);

			if (it) {
				ObjectPtr<String> other = value_to_string(it);
				return string_concat(self, other);
			}
			return self;
		}

		static VALUE string_get_size(const CallFrame* here, VALUE self, VALUE it) {
			if (is_string(self)) {
				return integer_to_value((int64_t)string_size(self));
			}
			return NULL;
		}

		static VALUE string_get_length(const CallFrame* here, VALUE self, VALUE it) {
			if (is_string(self)) {
				return integer_to_value((int64_t)string_length(self));
			}
			return NULL;
		}
	}

	ObjectPtr<Class> get_string_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("String"), get_type<String>());
			SN_DEFINE_METHOD(cls, "inspect", bindings::string_inspect);
			SN_DEFINE_METHOD(cls, "to_string", bindings::string_to_string);
			SN_DEFINE_METHOD(cls, "+", bindings::string_add);
			SN_DEFINE_PROPERTY(cls, "size", bindings::string_get_size, NULL);
			SN_DEFINE_PROPERTY(cls, "length", bindings::string_get_length, NULL);
			root = gc_create_root(cls);
		}
		return *root;
	}
}
