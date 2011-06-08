#include "snow/map.h"
#include "internal.h"

#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/numeric.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/type.h"

#include "lock.hpp"

#include "adapting_map.hpp"

namespace {
	void map_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		snow::AdaptingMap* map = (snow::AdaptingMap*)data;
		map->gc_each_root(callback);
	}
	
	SnObject* create_map_with_flags(uint32_t flags) {
		SnObject* map = snow_create_object(snow_get_map_class(), 0, NULL);
		snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		priv->flags = flags;
		return map;
	}
}

CAPI {
	SnObjectType SnMapType = {
		.data_size = sizeof(snow::AdaptingMap),
		.initialize = snow::construct<snow::AdaptingMap>,
		.finalize = snow::destruct<snow::AdaptingMap>,
		.copy = snow::assign<snow::AdaptingMap>,
		.gc_each_root = map_gc_each_root,
	};
	
	SnObject* snow_create_map() {
		return create_map_with_flags(snow::MAP_FLAT);
	}
	
	SnObject* snow_create_map_with_immediate_keys() {
		return create_map_with_flags(snow::MAP_IMMEDIATE_KEYS);
	}
	
	SnObject* snow_create_map_with_insertion_order() {
		return create_map_with_flags(snow::MAP_MAINTAINS_INSERTION_ORDER);
	}
	
	SnObject* snow_create_map_with_immediate_keys_and_insertion_order() {
		return create_map_with_flags(snow::MAP_IMMEDIATE_KEYS | snow::MAP_MAINTAINS_INSERTION_ORDER);
	}
	
	size_t snow_map_size(const SnObject* map) {
		SN_GC_SCOPED_RDLOCK(map);
		const snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		return priv->size();
	}
	
	VALUE snow_map_get(const SnObject* map, VALUE key) {
		SN_GC_SCOPED_RDLOCK(map);
		const snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		return priv->get(key);
	}
		
	VALUE snow_map_set(SnObject* map, VALUE key, VALUE value) {
		SN_GC_SCOPED_WRLOCK(map);
		snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		return priv->set(key, value);
	}
	
	VALUE snow_map_erase(SnObject* map, VALUE key) {
		SN_GC_SCOPED_WRLOCK(map);
		snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		return priv->erase(key);
	}
	
	void snow_map_reserve(SnObject* map, size_t size) {
		SN_GC_SCOPED_WRLOCK(map);
		snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		priv->reserve(size);
	}

	size_t snow_map_get_pairs(const SnObject* map, SnKeyValuePair* pairs, size_t max) {
		SN_GC_SCOPED_RDLOCK(map);
		const snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		return priv->get_pairs(pairs, max);
	}
	
	void snow_map_clear(SnObject* map, bool free_allocated_memory) {
		SN_GC_SCOPED_WRLOCK(map);
		snow::AdaptingMap* priv = snow::object_get_private<snow::AdaptingMap>(map, SnMapType);
		ASSERT(priv);
		priv->clear(free_allocated_memory);
	}
}


static VALUE map_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Map, inspect);
	SnObject* map = (SnObject*)self;
	const size_t size = snow_map_size(map);
	SnObject* result = snow_create_string_constant("#(");
	SnKeyValuePair* pairs = (SnKeyValuePair*)alloca(sizeof(SnKeyValuePair)*size);
	snow_map_get_pairs(map, pairs, size);
	for (size_t i = 0; i < size; ++i) {
		snow_string_append(result, snow_value_inspect(pairs[i][0]));
		snow_string_append_cstr(result, " => ");
		snow_string_append(result, snow_value_inspect(pairs[i][1]));
		if (i != size-1) snow_string_append_cstr(result, ", ");
	}
	snow_string_append_cstr(result, ")");
	return result;
}

static VALUE map_index_get(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Map, get);
	return snow_map_get((SnObject*)self, it);
}

static VALUE map_index_set(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Map, set);
	return snow_map_set((SnObject*)self, it, here->locals[1]);
}

static VALUE map_each_pair(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Map, each_pair);
	const SnObject* s = (const SnObject*)self;
	size_t sz = snow_map_size(s);
	SnKeyValuePair pairs[sz];
	snow_map_get_pairs(s, pairs, sz);
	for (size_t i = 0; i < sz; ++i) {
		snow_call(it, NULL, 2, pairs[i]);
	}
	return SN_NIL;
}

static VALUE map_initialize(const SnCallFrame* here, VALUE self, VALUE it) {
	const SnArguments* args = &here->args;
	if ((args->size - args->num_names) % 2) {
		snow_throw_exception_with_description("Cannot create map with odd number (%u) of arguments.", (uint32_t)(args->size - args->num_names));
	}
	
	bool immediate_keys = true;
	// check if any key is not an immediate
	for (size_t i = args->num_names; i < args->size; i += 2) {
		if (snow_is_object(args->data[i])) {
			immediate_keys = false;
			break;
		}
	}
	
	SnObject* map = (SnObject*)self;
	snow::AdaptingMap* priv = snow::value_get_private<snow::AdaptingMap>(self, SnMapType);
	if (!priv) snow_throw_exception_with_description("Map#initialized with non-map as self: %p.", self);
	
	// Slightly hack-ish, needed because we can't yet convert from immediate to non-immediate map.
	ASSERT(priv->size() == 0);
	ASSERT(priv->flags == snow::MAP_FLAT);
	if (immediate_keys) {
		priv->flags |= snow::MAP_IMMEDIATE_KEYS;
	}
	
	size_t num_pairs = (args->size - args->num_names) / 2 + args->num_names;
	snow_map_reserve(map, num_pairs);
	
	for (size_t i = 0; i < args->num_names; ++i) {
		snow_map_set(map, snow_symbol_to_value(args->names[i]), args->data[i]);
	}
	
	for (size_t i = args->num_names; i < args->size; i += 2) {
		snow_map_set(map, args->data[i], args->data[i+1]);
	}
	
	return self;
}

static VALUE map_get_size(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Map, each_pair);
	return snow_integer_to_value(snow_map_size((SnObject*)self));
}

CAPI SnObject* snow_get_map_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class_for_type(snow_sym("Map"), &SnMapType);
		snow_class_define_method(cls, "initialize", map_initialize);
		snow_class_define_method(cls, "inspect", map_inspect);
		snow_class_define_method(cls, "to_string", map_inspect);
		snow_class_define_method(cls, "get", map_index_get);
		snow_class_define_method(cls, "set", map_index_set);
		snow_class_define_method(cls, "each_pair", map_each_pair);
		snow_class_define_property(cls, "size", map_get_size, NULL);
		root = snow_gc_create_root(cls);
	}
	return *root;
}
