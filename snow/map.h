#pragma once
#ifndef MAP_H_MXEPAZB9
#define MAP_H_MXEPAZB9

#include "snow/value.h"

struct SnType;
struct SnObject;

CAPI const struct SnType* snow_get_map_type();

typedef struct SnMapRef {
	struct SnObject* obj;
} SnMapRef;

CAPI SnMapRef snow_create_map(SN_P);
CAPI size_t snow_map_size(SN_P, SnMapRef);
CAPI VALUE snow_map_get(SN_P, SnMapRef, VALUE key);
CAPI VALUE snow_map_set(SN_P, SnMapRef, VALUE key, VALUE value);
CAPI VALUE snow_map_erase(SN_P, SnMapRef, VALUE key);

#endif /* end of include guard: MAP_H_MXEPAZB9 */
