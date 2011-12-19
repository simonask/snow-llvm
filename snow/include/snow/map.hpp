#pragma once
#ifndef MAP_H_MXEPAZB9
#define MAP_H_MXEPAZB9

#include "snow/value.hpp"
#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Map;
	struct Class;
	typedef const ObjectPtr<Map>& MapPtr;
	typedef const ObjectPtr<const Map>& MapConstPtr;
	
	ObjectPtr<Map> create_map();
	ObjectPtr<Map> create_map_with_immediate_keys();  // i.e., will never call .hash on keys
	ObjectPtr<Map> create_map_with_insertion_order(); // i.e., will never be a full-blown hash map
	ObjectPtr<Map> create_map_with_immediate_keys_and_insertion_order();

	size_t map_size(MapConstPtr);
	VALUE map_get(MapConstPtr, VALUE key);
	VALUE map_set(MapPtr, VALUE key, VALUE value);
	VALUE map_erase(MapPtr, VALUE key);
	void map_reserve(MapPtr, size_t num_items);
	void map_clear(MapPtr, bool free_allocated_memory);
	typedef VALUE SnKeyValuePair[2];
	size_t map_get_pairs(MapConstPtr, SnKeyValuePair* tuples, size_t max);

	ObjectPtr<Class> get_map_class();
}

#endif /* end of include guard: MAP_H_MXEPAZB9 */
