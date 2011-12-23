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
	struct Class;
	typedef ObjectPtr<const String> StringConstPtr;
	typedef ObjectPtr<String> StringPtr;
	
	template <typename T> class LinkBuffer;
	
	bool is_string(Value);
	ObjectPtr<String> create_string(const char* utf8);
	ObjectPtr<String> create_string_constant(const char* utf8);
	ObjectPtr<String> create_string_with_size(const char* utf8, size_t size);
	ObjectPtr<String> create_string_from_linkbuffer(const LinkBuffer<char>& buf); // TODO: Remove
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
	
	ObjectPtr<Class> get_string_class();
}

#endif /* end of include guard: STRING_H_OYTL2E1P */
