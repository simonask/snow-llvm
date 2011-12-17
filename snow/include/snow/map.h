#pragma once
#ifndef MAP_H_MXEPAZB9
#define MAP_H_MXEPAZB9

#include "snow/value.h"
#include "snow/object.hpp"

CAPI SnObject* snow_create_map();
CAPI SnObject* snow_create_map_with_immediate_keys();  // i.e., will never call .hash on keys
CAPI SnObject* snow_create_map_with_insertion_order(); // i.e., will never be a full-blown hash map
CAPI SnObject* snow_create_map_with_immediate_keys_and_insertion_order();

CAPI size_t snow_map_size(const SnObject*);
CAPI VALUE snow_map_get(const SnObject*, VALUE key);
CAPI VALUE snow_map_set(SnObject*, VALUE key, VALUE value);
CAPI VALUE snow_map_erase(SnObject*, VALUE key);
CAPI void snow_map_reserve(SnObject*, size_t num_items);
CAPI void snow_map_clear(SnObject*, bool free_allocated_memory);
typedef VALUE SnKeyValuePair[2];
CAPI size_t snow_map_get_pairs(const SnObject*, SnKeyValuePair* tuples, size_t max);

CAPI struct SnObject* snow_get_map_class();

#endif /* end of include guard: MAP_H_MXEPAZB9 */
