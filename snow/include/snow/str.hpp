#ifndef STRING_H_OYTL2E1P
#define STRING_H_OYTL2E1P

#include "snow/basic.h"
#include "snow/object.hpp"
#include "snow/objectptr.hpp"
#include <stdarg.h>

struct SnLinkBuffer;

namespace snow {
	struct String;
	struct Array;
	typedef const ObjectPtr<const String>& StringConstPtr;
	typedef const ObjectPtr<String>& StringPtr;
	
	bool is_string(VALUE);
	ObjectPtr<String> create_string(const char* utf8);
	ObjectPtr<String> create_string_constant(const char* utf8);
	ObjectPtr<String> create_string_with_size(const char* utf8, size_t size);
	ObjectPtr<String> create_string_from_linkbuffer(SnLinkBuffer* buf); // TODO: Remove
	inline ObjectPtr<String> create_empty_string() { return create_string_constant(""); }
	ObjectPtr<String> string_concat(StringConstPtr a, StringConstPtr b);
	void string_append(StringPtr str, StringConstPtr other);
	void string_append_cstr(StringPtr str, const char* utf8);
	size_t string_copy_to(StringConstPtr str, char* buffer, size_t max);
	ObjectPtr<Array> string_split(StringConstPtr str, StringConstPtr separator);
	
	ObjectPtr<String> string_format(const char* utf8_format, ...);
	ObjectPtr<String> string_format_va(const char* utf8_format, va_list ap);
	
	size_t string_size(StringConstPtr str);
	size_t string_length(StringConstPtr str);
	
	SnObject* get_string_class();
}

#endif /* end of include guard: STRING_H_OYTL2E1P */