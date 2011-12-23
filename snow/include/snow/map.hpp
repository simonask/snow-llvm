#pragma once
#ifndef MAP_H_MXEPAZB9
#define MAP_H_MXEPAZB9

#include "snow/value.hpp"
#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Map;
	struct Class;
	typedef ObjectPtr<Map> MapPtr;
	typedef ObjectPtr<const Map> MapConstPtr;
	
	ObjectPtr<Map> create_map();
	ObjectPtr<Map> create_map_with_immediate_keys();  // i.e., will never call .hash on keys
	ObjectPtr<Map> create_map_with_insertion_order(); // i.e., will never be a full-blown hash map
	ObjectPtr<Map> create_map_with_immediate_keys_and_insertion_order();

	size_t map_size(MapConstPtr);
	Value map_get(MapConstPtr, Value key);
	Value& map_set(MapPtr, Value key, Value value);
	Value map_erase(MapPtr, Value key);
	void map_reserve(MapPtr, size_t num_items);
	void map_clear(MapPtr, bool free_allocated_memory);
	struct KeyValuePair { Value pair[2]; };
	size_t map_get_pairs(MapConstPtr, KeyValuePair* tuples, size_t max);

	ObjectPtr<Class> get_map_class();
}

#endif /* end of include guard: MAP_H_MXEPAZB9 */
