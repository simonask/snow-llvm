#pragma once
#ifndef MAP_H_MXEPAZB9
#define MAP_H_MXEPAZB9

#include "snow/value.h"
#include "snow/object.h"

struct SnObject;

typedef struct SnMap {
	SnObjectBase base;
	union {
		/*
			Snow Maps are adaptive. Below a certain threshold, they are a flat, associative list.
			Above the threshold, they turn into full-blown hash maps.
		*/
		
		struct {
			VALUE* keys;
			VALUE* values;
			uint32_t size;
			uint32_t alloc_size;
		} flat;
		
		void* hash_map;
	};
	uint32_t flags;
} SnMap;

CAPI SnMap* snow_create_map();
CAPI SnMap* snow_create_map_with_immediate_keys();  // i.e., will never call .hash on keys
CAPI SnMap* snow_create_map_with_insertion_order(); // i.e., will never be a full-blown hash map
CAPI SnMap* snow_create_map_with_immediate_keys_and_insertion_order();

CAPI size_t snow_map_size(const SnMap*);
CAPI VALUE snow_map_get(const SnMap*, VALUE key);
CAPI VALUE snow_map_set(SnMap*, VALUE key, VALUE value);
CAPI VALUE snow_map_erase(SnMap*, VALUE key);
CAPI void snow_map_reserve(SnMap*, size_t num_items);
CAPI void snow_map_get_keys_and_values(const SnMap*, VALUE* keys, VALUE* values);
typedef VALUE SnKeyValuePair[2];
CAPI void snow_map_get_pairs(const SnMap*, SnKeyValuePair* tuples);

// Used by the GC
typedef void(*SnMapForEachGCRootCallback)(VALUE);
CAPI void snow_map_for_each_gc_root(SnMap*, SnMapForEachGCRootCallback callback);
CAPI void snow_finalize_map(SnMap*);

#endif /* end of include guard: MAP_H_MXEPAZB9 */
