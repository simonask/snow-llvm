#include "snow/map.h"
#include "snow/type.h"
#include "snow/snow.h"
#include "snow/numeric.h"
#include "snow/boolean.h"
#include "snow/gc.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/str.h"

#include <google/dense_hash_map>
#include <stdlib.h>
#include <string.h>


namespace {
	// Use flat maps as long as the keys fit in 4 cache lines.
	// TODO: Benchmark this to find a better threshold.
	static const size_t FULL_HASH_THRESHOLD = 4 * SN_CACHE_LINE_SIZE / sizeof(VALUE);
	
	struct CallValueHash;
	struct CallValueEquals;
	
	typedef google::dense_hash_map<VALUE, VALUE> ImmediateHashMap;
	typedef google::dense_hash_map<VALUE, VALUE, CallValueHash, CallValueEquals> HashMap;
	
	enum MapFlags {
		MAP_FLAT                      = 1, // currently flat, but might become non-flat as it grows
		MAP_ALWAYS_FLAT               = 2, // never convert to non-flat -- fast get, slow insert/erase
		MAP_MAINTAINS_INSERTION_ORDER = 4, // slow (linear) get, fast insert/erase
		MAP_IMMEDIATE_KEYS            = 8, // all keys must be immediate values => fast comparison
	};
	
	inline bool map_is_flat(const SnMap* map) { return map->flags & MAP_FLAT; }
	inline bool map_is_always_flat(const SnMap* map) { return map->flags & (MAP_ALWAYS_FLAT | MAP_MAINTAINS_INSERTION_ORDER); }
	inline bool map_maintains_insertion_order(const SnMap* map) { return map->flags & MAP_ALWAYS_FLAT; }
	inline bool map_has_immediate_keys(const SnMap* map) { return map->flags & MAP_IMMEDIATE_KEYS; }
	
	inline HashMap* hash_map(SnMap* map) { return reinterpret_cast<HashMap*>(map->hash_map); }
	inline const HashMap* hash_map(const SnMap* map) { return reinterpret_cast<const HashMap*>(map->hash_map); }
	inline ImmediateHashMap* immediate_hash_map(SnMap* map) { return reinterpret_cast<ImmediateHashMap*>(map->hash_map); }
	inline const ImmediateHashMap* immediate_hash_map(const SnMap* map) { return reinterpret_cast<const ImmediateHashMap*>(map->hash_map); }
	
	struct CallValueHash {
	public:
		size_t operator()(VALUE v) const {
			if (snow_is_object(v)) {
				VALUE h = snow_call(snow_get_method(v, snow_sym("hash")), v, 0, NULL);
				ASSERT(snow_is_integer(h));
				return (size_t)h;
			} else {
				return (size_t)v;
			}
		}
	};
	
	struct CallValueEquals {
	public:
		bool operator()(const VALUE _a, const VALUE _b) const {
			VALUE a = const_cast<VALUE>(_a);
			VALUE b = const_cast<VALUE>(_b);
			VALUE r = snow_call(snow_get_method(a, snow_sym("=")), a, 1, &b);
			return snow_value_to_boolean(r);
		}
	};
	
	void flat_map_reserve(SnMap* map, uint32_t new_alloc_size) {
		ASSERT(map_is_flat(map));
		if (new_alloc_size > map->flat.alloc_size) {
			size_t n = new_alloc_size - map->flat.alloc_size;
			map->flat.keys = (VALUE*)realloc(map->flat.keys, new_alloc_size*sizeof(VALUE));
			memset(map->flat.keys + map->flat.alloc_size, 0, n);
			map->flat.values = (VALUE*)realloc(map->flat.values, new_alloc_size*sizeof(VALUE));
			memset(map->flat.values + map->flat.alloc_size, 0, n);
			map->flat.alloc_size = new_alloc_size;
		}
	}
	
	void convert_from_flat_to_full(SnMap* map) {
		ASSERT(map_is_flat(map));
		ASSERT(!map_maintains_insertion_order(map));
		ASSERT(!map_is_always_flat(map));
		if (map_has_immediate_keys(map)) {
			ImmediateHashMap* hmap = new ImmediateHashMap;
			hmap->set_empty_key(NULL);
			hmap->set_deleted_key(SN_NIL);
			for (uint32_t i = 0; i < map->flat.size; ++i) {
				(*hmap)[map->flat.keys[i]] = map->flat.values[i];
			}
			free(map->flat.keys);
			free(map->flat.values);
			map->hash_map = hmap;
		} else {
			HashMap* hmap = new HashMap;
			hmap->set_empty_key(NULL);
			hmap->set_deleted_key(SN_NIL);
			for (uint32_t i = 0; i < map->flat.size; ++i) {
				(*hmap)[map->flat.keys[i]] = map->flat.values[i];
			}
			free(map->flat.keys);
			free(map->flat.values);
			map->hash_map = hmap;
		}
		map->flags &= ~MAP_FLAT;
	}
	
	bool map_elements_are_equal(const SnMap* map, VALUE a, VALUE b) {
		if (map_has_immediate_keys(map)) {
			return a == b;
		} else {
			return CallValueEquals()(a, b);
		}
	}
	
	int compare_immediate_values(const void* _a, const void* _b) {
		const VALUE* a = (const VALUE*)_a;
		const VALUE* b = (const VALUE*)_b;
		return (int)((intptr_t)*a - (intptr_t)*b);
	}
	
	int compare_objects(const void* _a, const void* _b) {
		const VALUE* a = (const VALUE*)_a;
		const VALUE* b = (const VALUE*)_b;
		VALUE difference = snow_call(snow_get_method(*a, snow_sym("compare")), *a, 1, b);
		ASSERT(snow_is_integer(difference));
		return snow_value_to_integer(difference);
	}
	
	int map_elements_compare(const SnMap* map, VALUE a, VALUE b) {
		if (map_has_immediate_keys(map)) {
			return compare_immediate_values(&a, &b);
		} else {
			return compare_objects(&a, &b);
		}
	}
	
	int32_t flat_find_index(const SnMap* map, VALUE key) {
		ASSERT(map_is_flat(map));
		if (map_maintains_insertion_order(map)) {
			// linear search
			for (int32_t i = 0; i < map->flat.size; ++i) {
				if (map_elements_are_equal(map, key, map->flat.keys[i]))
					return i;
			}
		} else {
			// binary search
			VALUE* result;
			
			if (map_has_immediate_keys(map))
				result = (VALUE*)bsearch(&key, map->flat.keys, map->flat.size, sizeof(VALUE), compare_immediate_values);
			else
				result = (VALUE*)bsearch(&key, map->flat.keys, map->flat.size, sizeof(VALUE), compare_objects);
				
			if (result)
				return (int32_t)((VALUE*)result - map->flat.keys);
		}
		return -1; // not found
	}
}

CAPI {
	SnMap* snow_create_map() {
		SnMap* map = (SnMap*)snow_gc_alloc_object(SnMapType);
		memset(map, 0, sizeof(SnMap));
		map->flags = MAP_FLAT;
		return map;
	}
	
	SnMap* snow_create_map_with_immediate_keys() {
		SnMap* map = snow_create_map();
		map->flags |= MAP_IMMEDIATE_KEYS;
		return map;
	}
	
	SnMap* snow_create_map_with_insertion_order() {
		SnMap* map = snow_create_map();
		map->flags |= MAP_MAINTAINS_INSERTION_ORDER;
		return map;
	}
	
	SnMap* snow_create_map_with_immediate_keys_and_insertion_order() {
		SnMap* map = snow_create_map_with_immediate_keys();
		map->flags |= MAP_MAINTAINS_INSERTION_ORDER;
		return map;
	}
	
	size_t snow_map_size(const SnMap* map) {
		if (map_is_flat(map)) {
			return map->flat.size;
		} else if (map_has_immediate_keys(map)) {
			return immediate_hash_map(map)->size();
		} else {
			return hash_map(map)->size();
		}
	}
	
	VALUE snow_map_get(const SnMap* map, VALUE key) {
		if (map_is_flat(map)) {
			int32_t i = flat_find_index(map, key);
			if (i >= 0) return map->flat.values[i];
			return NULL;
		} else if (map_has_immediate_keys(map)) {
			const ImmediateHashMap* hmap = immediate_hash_map(map);
			ImmediateHashMap::const_iterator it = hmap->find(key);
			if (it != hmap->end()) return NULL;
			return it->second;
		} else {
			const HashMap* hmap = hash_map(map);
			HashMap::const_iterator it = hmap->find(key);
			if (it != hmap->end()) return NULL;
			return it->second;
		}
	}
	
	VALUE snow_map_set(SnMap* map, VALUE key, VALUE value) {
		if (map_is_flat(map)) {
			if (map_has_immediate_keys(map) && snow_is_object(key)) {
				snow_throw_exception_with_description("Attempted to use an Object as key in an immediates-only hash map.");
			}
			
			// see if the key already exists in the flat map
			int32_t i = flat_find_index(map, key);
			if (i >= 0) {
				map->flat.values[i] = value;
				return value;
			}
			
			// no, then we need to insert it
			// first, check if we need to convert to a full hash
			if (!map_is_always_flat(map) && map->flat.size > FULL_HASH_THRESHOLD) {
				convert_from_flat_to_full(map);
				return snow_map_set(map, key, value); // no longer flat, should hit one of the full-hash cases
			}
			
			// no, so insert it in the list
			
			flat_map_reserve(map, map->flat.size+1);
			if (map_maintains_insertion_order(map)) {
				// append at the end
				uint32_t i = map->flat.size++;
				map->flat.keys[i] = key;
				map->flat.values[i] = value;
				return value;
			} else {
				// insert in sorted order
				// TODO: Use binary search for this
				uint32_t insertion_point = 0;
				while (insertion_point < map->flat.size && map_elements_compare(map, map->flat.keys[insertion_point], key) < 0) {
					++insertion_point;
				}
				// shift all keys and values right
				size_t to_move = map->flat.size - insertion_point;
				if (to_move) {
					memmove(map->flat.keys + insertion_point + 1, map->flat.keys + insertion_point, to_move*sizeof(VALUE));
					memmove(map->flat.values + insertion_point + 1, map->flat.values + insertion_point, to_move*sizeof(VALUE));
				}
				++map->flat.size;
				map->flat.keys[insertion_point] = key;
				map->flat.values[insertion_point] = value;
				
				return value;
			}
		} else if (map_has_immediate_keys(map)) {
			ImmediateHashMap* hmap = immediate_hash_map(map);
			(*hmap)[key] = value;
			return value;
		} else {
			HashMap* hmap = hash_map(map);
			(*hmap)[key] = value;
			return value;
		}
	}
	
	VALUE snow_map_erase(SnMap* map, VALUE key) {
		// TODO: Consider if the map should be converted back into a flat map below the threshold?
		
		if (map_is_flat(map)) {
			int32_t found_idx = -1;
			for (int32_t i = 0; i < map->flat.size; ++i) {
				if (map->flat.keys[i] == key) {
					found_idx = i;
					break;
				}
			}
			
			if (found_idx >= 0) {
				// shift tail
				VALUE v = map->flat.values[found_idx];
				for (int32_t i = found_idx+1; i < map->flat.size; ++i) {
					map->flat.keys[i-1] = map->flat.keys[i];
					map->flat.values[i-1] = map->flat.values[i];
				}
				--map->flat.size;
				return v;
			}
			return NULL;
		} else if (map_has_immediate_keys(map)) {
			ImmediateHashMap* hmap = immediate_hash_map(map);
			ImmediateHashMap::iterator it = hmap->find(key);
			if (it != hmap->end()) {
				VALUE v = it->second;
				hmap->erase(it);
				return v;
			}
			return NULL;
		} else {
			HashMap* hmap = hash_map(map);
			HashMap::iterator it = hmap->find(key);
			if (it != hmap->end()) {
				VALUE v = it->second;
				hmap->erase(it);
				return v;
			}
			return NULL;
		}
	}
	
	void snow_map_reserve(SnMap* map, size_t size) {
		if (size > FULL_HASH_THRESHOLD) {
			convert_from_flat_to_full(map);
		}
		
		if (map_is_flat(map)) {
			flat_map_reserve(map, size);
		} else if (map_has_immediate_keys(map)) {
			ImmediateHashMap* hmap = immediate_hash_map(map);
			hmap->resize(size);
		} else {
			HashMap* hmap = hash_map(map);
			hmap->resize(size);
		}
	}
	

}


static VALUE map_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	char buffer[100];
	snprintf(buffer, 100, "[Map@%p]", self);
	return snow_create_string(buffer);
}

static VALUE map_index_get(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnMapType);
	return snow_map_get((SnMap*)self, it);
}

static VALUE map_index_set(SnFunctionCallContext* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnMapType);
	return snow_map_set((SnMap*)self, it, here->locals[1]);
}

CAPI SnObject* snow_create_map_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", map_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", map_inspect, 0);
	SN_DEFINE_METHOD(proto, "__index_get__", map_index_get, 1);
	SN_DEFINE_METHOD(proto, "__index_set__", map_index_set, 2);
	return proto;
}
