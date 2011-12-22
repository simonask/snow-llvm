#pragma once
#ifndef ADAPTING_MAP_HPP_GHRARLGI
#define ADAPTING_MAP_HPP_GHRARLGI

#include "snow/basic.h"
#include "snow/exception.hpp"
#include "snow/util.hpp"

#include <google/dense_hash_map>
#include <algorithm>
#include <string.h>

namespace std {
	template <>
	struct hash<snow::Immediate> {
		int operator()(snow::Immediate imm) const {
			return std::hash<snow::VALUE>()(imm);
		};
	};
}

namespace snow {
	/*
		Snow Maps are adaptive. Below a certain threshold, they are a flat, associative list.
		Above the threshold, they turn into full-blown hash maps.
	*/
	// TODO: Benchmark this to find a better threshold.
	static const size_t FULL_HASH_THRESHOLD = 4 * SN_CACHE_LINE_SIZE / sizeof(Value);
	
	struct CallValueHash;
	struct CallNonImmediateValueEquals;

	
	typedef google::dense_hash_map<Immediate, Value> ImmediateHashMap;
	typedef google::dense_hash_map<Value, Value, CallValueHash, CallNonImmediateValueEquals> HashMap;
	
	enum MapFlags {
		MAP_FLAT                      = 1, // currently flat, but might become non-flat as it grows
		MAP_ALWAYS_FLAT               = 2, // never convert to non-flat -- fast get, slow insert/erase
		MAP_MAINTAINS_INSERTION_ORDER = 4, // slow (linear) get, fast insert/erase
		MAP_IMMEDIATE_KEYS            = 8, // all keys must be immediate values => fast comparison
	};
	
	struct AdaptingMap {
		union {
			struct {
				Value* keys;
				Value* values;
				uint32_t size;
				uint32_t alloc_size;
			} flat;

			void* hash_map;
		};
		uint32_t flags;
		
		AdaptingMap() {
			flat.keys = NULL;
			flat.values = NULL;
			flat.size = 0;
			flat.alloc_size = 0;
			flags = MAP_FLAT;
		}
		~AdaptingMap() {
			clear(true);
		}
		
		bool is_flat() const { return flags & MAP_FLAT; }
		bool is_always_flat() const { return flags & (MAP_ALWAYS_FLAT | MAP_MAINTAINS_INSERTION_ORDER); }
		bool maintains_insertion_order() const { return MAP_MAINTAINS_INSERTION_ORDER; }
		bool has_immediate_keys() const { return flags & MAP_IMMEDIATE_KEYS; }
		
		inline void reserve(uint32_t new_alloc_size);
		inline size_t size() const;
		inline Value get(const Value& key) const;
		inline Value& set(const Value& key, const Value& value);
		inline Value erase(const Value& key);
		inline size_t get_pairs(KeyValuePair* pairs, size_t max) const;
		inline void clear(bool free_allocated_memory);
	private:
		HashMap* as_hash_map() { return reinterpret_cast<HashMap*>(hash_map); }
		const HashMap* as_hash_map() const { return reinterpret_cast<const HashMap*>(hash_map); }
		ImmediateHashMap* as_immediate_hash_map() { return reinterpret_cast<ImmediateHashMap*>(hash_map); }
		const ImmediateHashMap* as_immediate_hash_map() const { return reinterpret_cast<const ImmediateHashMap*>(hash_map); }
		
		inline void convert_from_flat_to_full();
		template <typename T> inline void convert_flat_to_full();
		int32_t flat_find_index(const Value& key) const;
	};
	
	struct CallValueHash {
		size_t operator()(const Value& v) const {
			Value hash = SN_CALL_METHOD(v, "hash", 0, NULL);
			if (!hash.is_integer()) throw_exception_with_description("A hash method for object %p returned a non-integer (%p).", v.value(), hash.value());
			return (size_t)value_to_integer(hash);
		}
	};
	
	struct CallValueCompare {
		const AdaptingMap* map;
		explicit CallValueCompare(const AdaptingMap& map) : map(&map) {}
		int64_t operator()(const Value& a, const Value& b) const {
			if (map->has_immediate_keys()) {
				return (int64_t)((intptr_t)a.value() - (intptr_t)b.value());
			} else {
				Value diff = SN_CALL_METHOD(a, "<=>", 1, &b);
				if (!diff.is_integer()) throw_exception_with_description("A comparison method (<=>) for object %p returned a non-integer (%p).", a.value(), diff.value());
				return value_to_integer(diff);
			}
		}
	};
	
	struct CallNonImmediateValueEquals {
		bool operator()(const Value& a, const Value& b) const {
			Value truth = SN_CALL_METHOD(a, "=", 1, &b);
			return is_truthy(truth);
		}
	};
	
	struct CallValueEquals {
		CallValueCompare cmp;
		explicit CallValueEquals(const AdaptingMap& map) : cmp(map) {}
		bool operator()(const Value& _a, const Value& _b) const {
			return cmp(_a, _b) == 0;
		}
	};
	
	struct CallValueLessThan {
		CallValueCompare cmp;
		explicit CallValueLessThan(const AdaptingMap& map) : cmp(map) {}
		bool operator()(const Value& _a, const Value& _b) const {
			return cmp(_a, _b) < 0;
		}
	};
	
	inline size_t AdaptingMap::size() const {
		if (is_flat()) {
			return flat.size;
		} else if (has_immediate_keys()) {
			return as_immediate_hash_map()->size();
		} else {
			return as_hash_map()->size();
		}
	}
	
	inline void AdaptingMap::reserve(uint32_t new_alloc_size) {
		if (!is_always_flat() && new_alloc_size > FULL_HASH_THRESHOLD) {
			convert_from_flat_to_full();
		}
		
		if (is_flat()) {
			if (new_alloc_size > flat.alloc_size) {
				size_t n = new_alloc_size - flat.alloc_size;
				flat.keys = snow::realloc_range(flat.keys, flat.alloc_size, new_alloc_size);
				snow::assign_range<Value>(flat.keys + flat.alloc_size, NULL, n);
				flat.values = snow::realloc_range(flat.values, flat.alloc_size, new_alloc_size);
				snow::assign_range<Value>(flat.values + flat.alloc_size, NULL, n);
				flat.alloc_size = new_alloc_size;
			}
		} else if (has_immediate_keys()) {
			as_immediate_hash_map()->resize(new_alloc_size);
		} else {
			as_hash_map()->resize(new_alloc_size);
		}
	}
	
	inline int32_t AdaptingMap::flat_find_index(const Value& key) const {
		ASSERT(is_flat());
		if (maintains_insertion_order()) {
			// linear search
			for (int32_t i = 0; i < flat.size; ++i) {
				if (CallValueEquals(*this)(key, flat.keys[i]))
					return i;
			}
			return -1;
		} else {
			// binary search
			if (has_immediate_keys() && !is_immediate(key)) return -1;
			Value* end = flat.keys + flat.size;
			Value* result = std::lower_bound(flat.keys, end, key, CallValueLessThan(*this));
			if (result == end || !CallValueEquals(*this)(key, *result)) return -1;
			return (int32_t)(result - flat.keys);
		}
	}
	
	inline Value AdaptingMap::get(const Value& key) const {
		if (is_flat()) {
			int32_t i = flat_find_index(key);
			if (i >= 0) return flat.values[i];
			return NULL;
		} else if (has_immediate_keys()) {
			const ImmediateHashMap* hmap = as_immediate_hash_map();
			ImmediateHashMap::const_iterator it = hmap->find(key);
			if (it == hmap->end()) return NULL;
			return it->second;
		} else {
			const HashMap* hmap = as_hash_map();
			HashMap::const_iterator it = hmap->find(key);
			if (it == hmap->end()) return NULL;
			return it->second;
		}
	}
	
	inline Value& AdaptingMap::set(const Value& key, const Value& value) {
		if (has_immediate_keys() && is_object(key)) {
			// TODO: Convert from immediate-only to regular map?
			throw_exception_with_description("Attempted to use an object as key in an immediates-only hash map.");
		}
		if (is_flat()) {
			if (maintains_insertion_order()) {
				for (size_t i = 0; i < flat.size; ++i) {
					if (CallValueEquals(*this)(key, flat.keys[i])) {
						flat.values[i] = value;
						return flat.values[i];
					}
				}
				
				if (!is_always_flat() && flat.size >= FULL_HASH_THRESHOLD) {
					convert_from_flat_to_full();
					return set(key, value);
				}
				
				// append at end
				reserve(flat.size + 1);
				uint32_t idx = flat.size++;
				flat.keys[idx] = key;
				flat.values[idx] = value;
				return flat.values[idx];
			} else {
				// Insert or change by binary search.
				Value* end = flat.keys + flat.size;
				Value* found = std::lower_bound(flat.keys, end, key, CallValueLessThan(*this));
				uint32_t insertion_point = found - flat.keys;
				if (found != end && CallValueEquals(*this)(*found, key)) {
					flat.values[insertion_point] = value;
					return flat.values[insertion_point];
				}
				
				reserve(flat.size + 1);
				size_t to_move = flat.size - insertion_point;
				if (to_move) {
					snow::move_range(flat.keys   + insertion_point + 1, flat.keys   + insertion_point, to_move);
					snow::move_range(flat.values + insertion_point + 1, flat.values + insertion_point, to_move);
				}
				++flat.size;
				flat.keys[insertion_point] = key;
				flat.values[insertion_point] = value;
				return flat.values[insertion_point];
			}
		} else if (has_immediate_keys()) {
			Value& place = (*as_immediate_hash_map())[key];
			place = value;
			return place;
		} else {
			Value& place = (*as_hash_map())[key];
			place = value;
			return place;
		}
	}
	
	inline size_t AdaptingMap::get_pairs(KeyValuePair* pairs, size_t max) const {
		if (is_flat()) {
			size_t i;
			for (i = 0; i < flat.size && i < max; ++i) {
				pairs[i].pair[0] = flat.keys[i];
				pairs[i].pair[1] = flat.values[i];
			}
			return i;
		} else if (has_immediate_keys()) {
			const ImmediateHashMap* hmap = as_immediate_hash_map();
			size_t i = 0;
			for (ImmediateHashMap::const_iterator it = hmap->begin(); it != hmap->end() && i < max; ++it) {
				pairs[i].pair[0] = it->first;
				pairs[i].pair[1] = it->second;
				++i;
			}
			return i;
		} else {
			const HashMap* hmap = as_hash_map();
			size_t i = 0;
			for (HashMap::const_iterator it = hmap->begin(); it != hmap->end() && i < max; ++it) {
				pairs[i].pair[0] = it->first;
				pairs[i].pair[1] = it->second;
				++i;
			}
			return i;
		}
	}

	inline Value AdaptingMap::erase(const Value& key) {
		// TODO: Consider if the map should be converted back into a flat map below the threshold?
		if (is_flat()) {
			int32_t found_idx = flat_find_index(key);

			if (found_idx >= 0) {
				// shift tail
				Value v = flat.values[found_idx];
				snow::move_range(flat.keys   + found_idx, flat.keys   + found_idx + 1, flat.size - found_idx - 1);
				snow::move_range(flat.values + found_idx, flat.values + found_idx + 1, flat.size - found_idx - 1);
				--flat.size;
				return v;
			}
			return NULL;
		} else if (has_immediate_keys()) {
			ImmediateHashMap* hmap = as_immediate_hash_map();
			ImmediateHashMap::iterator it = hmap->find(key);
			if (it != hmap->end()) {
				Value v = it->second;
				hmap->erase(it);
				return v;
			}
			return NULL;
		} else {
			HashMap* hmap = as_hash_map();
			HashMap::iterator it = hmap->find(key);
			if (it != hmap->end()) {
				Value v = it->second;
				hmap->erase(it);
				return v;
			}
			return NULL;
		}
	}
	
	inline void AdaptingMap::clear(bool free_allocated_memory) {
		// TODO: Consider converting from full to flat.
		if (is_flat()) {
			flat.size = 0;
			if (free_allocated_memory) {
				snow::dealloc_range(flat.keys);
				snow::dealloc_range(flat.values);
				flat.alloc_size = 0;
			}
		} else if (has_immediate_keys()) {
			if (free_allocated_memory) {
				as_immediate_hash_map()->clear();
			} else {
				as_immediate_hash_map()->clear_no_resize();
			}
		} else {
			if (free_allocated_memory) {
				as_hash_map()->clear();
			} else {
				as_hash_map()->clear_no_resize();
			}
		}
	}
	
	inline void AdaptingMap::convert_from_flat_to_full() {
		ASSERT(is_flat());
		ASSERT(!is_always_flat());
		if (has_immediate_keys()) {
			convert_flat_to_full<ImmediateHashMap>();
		} else {
			convert_flat_to_full<HashMap>();
		}
		ASSERT(!is_flat());
	}
	
	template <typename T>
	inline void AdaptingMap::convert_flat_to_full() {
		ASSERT(is_flat());
		T* hmap = new T;
		hmap->set_empty_key(NULL);
		hmap->set_deleted_key(SN_NIL);
		for (uint32_t i = 0; i < flat.size; ++i) {
			(*hmap)[flat.keys[i]] = flat.values[i];
		}
		snow::dealloc_range(flat.keys);
		snow::dealloc_range(flat.values);
		flags &= ~MAP_FLAT;
		hash_map = hmap;
	}
}

#endif /* end of include guard: ADAPTING_MAP_HPP_GHRARLGI */
