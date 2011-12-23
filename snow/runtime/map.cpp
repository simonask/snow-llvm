#include "snow/map.hpp"
#include "internal.h"

#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/exception.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "snow/numeric.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"

#include "snow/objectptr.hpp"
#include "adapting_map.hpp"

namespace snow {
	struct Map : AdaptingMap {};
	
	SN_REGISTER_CPP_TYPE(Map, NULL)
	
	ObjectPtr<Map> create_map_with_flags(uint32_t flags) {
		ObjectPtr<Map> map = create_object(get_map_class(), 0, NULL);
		map->flags = flags;
		return map;
	}
	
	ObjectPtr<Map> create_map() {
		return create_map_with_flags(snow::MAP_FLAT);
	}
	
	ObjectPtr<Map> create_map_with_immediate_keys() {
		return create_map_with_flags(snow::MAP_IMMEDIATE_KEYS);
	}
	
	ObjectPtr<Map> create_map_with_insertion_order() {
		return create_map_with_flags(snow::MAP_MAINTAINS_INSERTION_ORDER);
	}
	
	ObjectPtr<Map> create_map_with_immediate_keys_and_insertion_order() {
		return create_map_with_flags(snow::MAP_IMMEDIATE_KEYS | snow::MAP_MAINTAINS_INSERTION_ORDER);
	}
	
	size_t map_size(MapConstPtr map) {
		return map->size();
	}
	
	Value map_get(MapConstPtr map, Value key) {
		return map->get(key);
	}
		
	Value& map_set(MapPtr map, Value key, Value value) {
		return map->set(key, value);
	}
	
	Value map_erase(MapPtr map, Value key) {
		return map->erase(key);
	}
	
	void map_reserve(MapPtr map, size_t size) {
		map->reserve(size);
	}

	size_t map_get_pairs(MapConstPtr map, KeyValuePair* pairs, size_t max) {
		return map->get_pairs(pairs, max);
	}
	
	void map_clear(MapPtr map, bool free_allocated_memory) {
		map->clear(free_allocated_memory);
	}
	
	namespace bindings {
		static VALUE map_inspect(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<const Map> map = self;
			const size_t size = map_size(map);
			ObjectPtr<String> result = create_string_constant("#(");
			SN_STACK_ARRAY(KeyValuePair, pairs, size);
			map_get_pairs(map, pairs, size);
			for (size_t i = 0; i < size; ++i) {
				string_append(result, value_inspect(pairs[i].pair[0]));
				string_append_cstr(result, " => ");
				string_append(result, value_inspect(pairs[i].pair[1]));
				if (i != size-1) string_append_cstr(result, ", ");
			}
			string_append_cstr(result, ")");
			return result;
		}

		static VALUE map_index_get(const CallFrame* here, VALUE self, VALUE it) {
			return map_get(self, it);
		}

		static VALUE map_index_set(const CallFrame* here, VALUE self, VALUE it) {
			return map_set(self, it, here->locals[1]);
		}

		static VALUE map_each_pair(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<const Map> s = self;
			size_t sz = map_size(s);
			SN_STACK_ARRAY(KeyValuePair, pairs, sz);
			snow::map_get_pairs(s, pairs, sz);
			for (size_t i = 0; i < sz; ++i) {
				call(it, NULL, 2, pairs[i].pair);
			}
			return SN_NIL;
		}

		static VALUE map_initialize(const CallFrame* here, VALUE self, VALUE it) {
			const SnArguments* args = here->args;
			if ((args->size - args->num_names) % 2) {
				throw_exception_with_description("Cannot create map with odd number (%u) of arguments.", (uint32_t)(args->size - args->num_names));
			}

			bool immediate_keys = true;
			// check if any key is not an immediate
			for (size_t i = args->num_names; i < args->size; i += 2) {
				if (is_object(args->data[i])) {
					immediate_keys = false;
					break;
				}
			}

			ObjectPtr<Map> map = self;
			if (map == NULL) throw_exception_with_description("Map#initialized with non-map as self: %p.", self);

			// Slightly hack-ish, needed because we can't yet convert from immediate to non-immediate map.
			ASSERT(map->size() == 0);
			ASSERT(map->flags == snow::MAP_FLAT);
			if (immediate_keys) {
				map->flags |= snow::MAP_IMMEDIATE_KEYS;
			}

			size_t num_pairs = (args->size - args->num_names) / 2 + args->num_names;
			map_reserve(map, num_pairs);

			for (size_t i = 0; i < args->num_names; ++i) {
				map_set(map, snow::symbol_to_value(args->names[i]), args->data[i]);
			}

			for (size_t i = args->num_names; i < args->size; i += 2) {
				map_set(map, args->data[i], args->data[i+1]);
			}

			return self;
		}

		static VALUE map_get_size(const CallFrame* here, VALUE self, VALUE it) {
			return integer_to_value(map_size(self));
		}
	}
	
	ObjectPtr<Class> get_map_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Map"), get_type<Map>());
			SN_DEFINE_METHOD(cls, "initialize", bindings::map_initialize);
			SN_DEFINE_METHOD(cls, "inspect", bindings::map_inspect);
			SN_DEFINE_METHOD(cls, "to_string", bindings::map_inspect);
			SN_DEFINE_METHOD(cls, "get", bindings::map_index_get);
			SN_DEFINE_METHOD(cls, "set", bindings::map_index_set);
			SN_DEFINE_METHOD(cls, "each_pair", bindings::map_each_pair);
			SN_DEFINE_PROPERTY(cls, "size", bindings::map_get_size, NULL);
			root = gc_create_root(cls);
		}
		return *root;
	}
	
}