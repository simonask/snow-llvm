#pragma once
#ifndef ARRAY_H_XVGE8JQI
#define ARRAY_H_XVGE8JQI

#include "snow/basic.h"
#include "snow/value.h"

struct SnType;
struct SnObject;

CAPI const struct SnType* snow_get_array_type();

typedef struct SnArrayRef {
	struct SnObject* obj;
} SnArrayRef;

CAPI SnArrayRef snow_create_array(SN_P);

CAPI size_t snow_array_size(const SnArrayRef array);
CAPI VALUE snow_array_get(const SnArrayRef array, int i);

CAPI void snow_array_reserve(SN_P, SnArrayRef array, uint32_t new_size);
CAPI VALUE snow_array_set(SN_P, SnArrayRef array, int i, VALUE val);
CAPI SnArrayRef snow_array_push(SN_P, SnArrayRef array, VALUE val);

static inline struct SnObject* snow_array_as_object(SnArrayRef array) { return array.obj; }
CAPI SnArrayRef snow_object_as_array(struct SnObject* obj);

#endif /* end of include guard: ARRAY_H_XVGE8JQI */
