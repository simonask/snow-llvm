#ifndef STRING_H_OYTL2E1P
#define STRING_H_OYTL2E1P

#include "snow/basic.h"
#include "snow/object.h"
#include <stdarg.h>

struct SnLinkBuffer;

typedef struct SnString {
	SnObject base;
	char* data; // null terminated
	uint32_t size;
	uint32_t length;
	bool constant; // never to be deleted
} SnString;

CAPI bool snow_is_string(VALUE val);
CAPI SnObject* snow_create_string(const char* utf8);
CAPI SnObject* snow_create_string_constant(const char* utf8);
CAPI SnObject* snow_create_string_with_size(const char* utf8, size_t size);
CAPI SnObject* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf); // TODO: Remove
INLINE SnObject* snow_create_empty_string() { return snow_create_string_constant(""); }
CAPI SnObject* snow_string_concat(const SnObject* a, const SnObject* b); // create new string
CAPI void snow_string_append(SnObject* str, const SnObject* other); // append to str
CAPI void snow_string_append_cstr(SnObject* str, const char* utf8);
CAPI size_t snow_string_copy_to(const SnObject* str, char* buffer, size_t max);
CAPI struct SnObject* snow_string_split(const SnObject* str, const SnObject* separator);

CAPI SnObject* snow_string_format(const char* utf8_format, ...);
CAPI SnObject* snow_string_format_va(const char* utf8_format, va_list ap);

CAPI size_t snow_string_size(const SnObject* str);
CAPI size_t snow_string_length(const SnObject* str);

CAPI struct SnObject* snow_get_string_class();

#endif /* end of include guard: STRING_H_OYTL2E1P */
