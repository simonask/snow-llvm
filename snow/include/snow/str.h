#ifndef STRING_H_OYTL2E1P
#define STRING_H_OYTL2E1P

#include "snow/basic.h"
#include "snow/object.h"

struct SnLinkBuffer;
struct SnType;

CAPI const struct SnType* snow_get_string_type();

typedef struct SnStringRef {
	SnObject* obj;
} SnStringRef;

CAPI SnStringRef snow_create_string(SN_P, const char* utf8);
CAPI SnStringRef snow_create_string_with_size(SN_P, const char* utf8, size_t size);
CAPI SnStringRef snow_create_string_from_linkbuffer(SN_P, struct SnLinkBuffer* buf);
CAPI SnStringRef snow_string_concat(SN_P, const SnStringRef a, const SnStringRef b);

CAPI const char* snow_string_cstr(const SnStringRef obj);
CAPI size_t snow_string_size(const SnStringRef str);
CAPI size_t snow_string_length(const SnStringRef str);

CAPI bool snow_object_is_string(const SnObject* obj);
CAPI SnStringRef snow_object_as_string(SnObject* obj);
static inline struct SnObject* snow_string_as_object(SnStringRef str) { return str.obj; }


#endif /* end of include guard: STRING_H_OYTL2E1P */
