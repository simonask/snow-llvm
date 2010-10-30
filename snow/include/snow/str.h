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
} SnString;

CAPI SnString* snow_create_string(const char* utf8);
CAPI SnString* snow_create_string_with_size(const char* utf8, size_t size);
CAPI SnString* snow_create_string_from_linkbuffer(struct SnLinkBuffer* buf);
CAPI SnString* snow_string_concat(const SnString* a, const SnString* b); // create new string
CAPI SnString* snow_string_append(SnString* str, const SnString* other); // append to str
CAPI struct SnArray* snow_string_split(const SnString* str, const SnString* separator);

CAPI SnString* snow_string_format(const char* utf8_format, ...);

static inline const char* snow_string_cstr(const SnString* str) { return str->data; }
static inline uint32_t snow_string_size(const SnString* str) { return str->size; }
static inline uint32_t snow_string_length(const SnString* str) { return str->length; }


#endif /* end of include guard: STRING_H_OYTL2E1P */
