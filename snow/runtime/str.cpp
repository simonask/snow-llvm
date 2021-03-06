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

#include <sstream>

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
	
	void preallocate_string_data(PreallocatedStringData& data, size_t sz) {
		ASSERT(data.data == NULL);
		if (sz) {
			data.data = new char[sz];
		}
	}
	
	ObjectPtr<String> create_string_from_preallocated_data(PreallocatedStringData& data) {
		ObjectPtr<String> str = create_object(get_string_class(), 0, NULL);
		str->size = data.size;
		str->data = data.data;
		str->length = get_utf8_length(data.data, data.size);
		str->constant = false;
		data.data = NULL;
		data.size = 0;
		return str;
	}

	ObjectPtr<String> create_string(const char* utf8) {
		return create_string_with_size(utf8, utf8 ? strlen(utf8) : 0);
	}

	ObjectPtr<String> create_string_constant(const char* utf8) {
		ObjectPtr<String> str = create_object(get_string_class(), 0, NULL);
		str->size = utf8 ? strlen(utf8) : 0;
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
		char* buffer = s ? new char[s] : NULL;
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
	
	size_t string_copy_to(StringConstPtr str, std::stringstream& buffer) {
		buffer.write(str->data, str->size);
		return str->size;
	}
	
	ObjectPtr<String> string_escape(StringConstPtr str) {
		std::stringstream ss;
		const char* end = str->data + str->size;
		for (const char* p = str->data; p < end; ++p) {
			switch (*p) {
				case '"':
				case '\\':
					ss << '\\';
				default: break;
			}
			ss << *p;
		}
		return create_string(ss.str().c_str());
	}

	size_t string_size(StringConstPtr str) {
		return str->size;
	}

	size_t string_length(StringConstPtr str) {
		return str->length;
	}
	
	void string_puts(StringConstPtr str) {
		fwrite(str->data, 1, str->size, stdout);
		fwrite("\n", 1, 1, stdout);
	}

	namespace bindings {
		static VALUE string_inspect(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_string(self)) throw_exception_with_description("String#inspect called on something that's not a string: %@.", self);
			ObjectPtr<String> s = self;
			return format_string("\"%@\"", string_escape(s));
		}

		static VALUE string_to_string(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_string(self)) throw_exception_with_description("String#to_string called on something that's not a string: %@.", self);
			return self;
		}

		static VALUE string_add(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_string(self)) throw_exception_with_description("String#+ called on something that's not a string: %@.", self);

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
