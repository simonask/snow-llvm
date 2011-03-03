#ifndef STRING_H_OYTL2E1P
#define STRING_H_OYTL2E1P

#include "snow/basic.h"
#include "snow/object.h"

struct SnLinkBuffer;
struct SnArray;

typedef struct SnString {
	SnObjectBase base;
	char* data; // null terminated
	uint32_t size;
	uint32_t length;
	bool constant; // never to be deleted
} SnString;

CAPI SnString* snow_create_string(const char* utf8);
CAPI SnString* snow_create_string_constant(const char* utf8);
CAPI SnString* snow_create_string_with_size(const char* utf8, size_t size);
CAPI SnString* snow_create_string_take_ownership(char* utf8_allocated_with_malloc);
CAPI SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf);
inline SnString* snow_create_empty_string() { return snow_create_string_constant(""); }
CAPI SnString* snow_string_concat(const SnString* a, const SnString* b); // create new string
CAPI void snow_string_append(SnString* str, const SnString* other); // append to str
CAPI void snow_string_append_cstr(SnString* str, const char* utf8);
CAPI size_t snow_string_copy_to(const SnString* str, char* buffer, size_t max);
CAPI struct SnArray* snow_string_split(const SnString* str, const SnString* separator);

CAPI SnString* snow_string_format(const char* utf8_format, ...);

INLINE const char* snow_string_cstr(const SnString* str) { return str->data; } // XXX: Inherently thread-unsafe!
INLINE size_t snow_string_size(const SnString* str);
INLINE size_t snow_string_length(const SnString* str);

CAPI void snow_finalize_string(SnString*);

#endif /* end of include guard: STRING_H_OYTL2E1P */
