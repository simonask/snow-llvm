#include "snow/map.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/gc.h"
#include <google/dense_hash_map>

#define GET_DATA(VAR, OBJ) SnMapData* VAR = (SnMapData*)(OBJ)->data;
#define GET_RDATA(VAR, REF) GET_DATA(VAR, (REF).obj)

namespace {
	struct SnMapData {
		google::dense_hash_map<VALUE, VALUE> map;
	};
	
	void map_initialize(SN_P p, SnObject* obj) {
		GET_DATA(data, obj);
		new(data) SnMapData;
		data->map.set_deleted_key(NULL);
		data->map.set_empty_key(SN_NIL);
	}
	
	void map_finalize(SN_P p, SnObject* obj) {
		GET_DATA(data, obj);
		data->~SnMapData();
	}
	
	void map_copy(SN_P p, SnObject* copy, const SnObject* original) {
		GET_DATA(a, copy);
		GET_DATA(b, original);
		*a = *b;
	}
	
	void map_for_each_root(SN_P p, VALUE self, SnForEachRootCallbackFunc func, void* userdata) {
		ASSERT(snow_is_object(self));
		SnObject* obj = (SnObject*)self;
		ASSERT(obj->type == snow_get_map_type());
		GET_DATA(data, obj);
		google::dense_hash_map<VALUE, VALUE>::iterator it;
		for (it = data->map.begin(); it != data->map.end(); ++it) {
			func(p, self, (VALUE*)&it->first, userdata);
			func(p, self, &it->second, userdata);
		}
	}
	
	void init_map_prototype(SN_P);
	VALUE map_get_member_fast(SN_P, VALUE self, SnSymbol sym);
	VALUE map_set_member_fast(SN_P, VALUE self, SnSymbol sym, VALUE val);
	VALUE map_call_fast(SN_P, VALUE functor, VALUE self, struct SnArguments*);
	
	VALUE map_get_member(SN_P p, VALUE self, SnSymbol sym) {
		init_map_prototype(p);
		return map_get_member_fast(p, self, sym);
	}
	
	VALUE map_set_member(SN_P p, VALUE self, SnSymbol sym, VALUE val) {
		init_map_prototype(p);
		return map_set_member_fast(p, self, sym, val);
	}
	
	VALUE map_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
		init_map_prototype(p);
		return map_call_fast(p, functor, self, args);
	}
	
	VALUE map_get_member_fast(SN_P p, VALUE self, SnSymbol sym) {
		return SN_NIL;
	}
	
	VALUE map_set_member_fast(SN_P p, VALUE self, SnSymbol sym, VALUE value) {
		return SN_NIL;
	}
	
	VALUE map_call_fast(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
		return SN_NIL;
	}

	static SnType MapType;
	
	void init_map_prototype(SN_P p) {
		MapType.get_member = map_get_member_fast;
		MapType.set_member = map_set_member_fast;
		MapType.call = map_call_fast;
	}
}

CAPI {
	const SnType* snow_get_map_type() {
		static const SnType* type = NULL;
		if (!type) {
			MapType.size = sizeof(SnMapData);
			MapType.initialize = map_initialize;
			MapType.finalize = map_finalize;
			MapType.copy = map_copy;
			MapType.for_each_root = map_for_each_root;
			MapType.get_member = map_get_member;
			MapType.set_member = map_set_member;
			MapType.call = map_call;
			type = &MapType;
		}
		return type;
	}
	
	SnMapRef snow_create_map(SN_P p) {
		SnObject* obj = snow_gc_create_object(p, snow_get_map_type());
		SnMapRef ref;
		ref.obj = obj;
		return ref;
	}
	
	size_t snow_map_size(SN_P p, SnMapRef r) {
		GET_RDATA(data, r);
		return data->map.size();
	}
	
	VALUE snow_map_get(SN_P p, SnMapRef r, VALUE key) {
		GET_RDATA(data, r);
		google::dense_hash_map<VALUE, VALUE>::iterator it = data->map.find(key);
		if (it == data->map.end()) {
			return NULL;
		}
		return it->second;
	}
	
	VALUE snow_map_set(SN_P p, SnMapRef r, VALUE key, VALUE value) {
		GET_RDATA(data, r);
		data->map[key] = value;
		return value;
	}
	
	VALUE snow_map_erase(SN_P p, SnMapRef r, VALUE key) {
		GET_RDATA(data, r);
		google::dense_hash_map<VALUE, VALUE>::iterator it = data->map.find(key);
		if (it == data->map.end()) {
			return NULL;
		}
		VALUE val = it->second;
		data->map.erase(it);
		return val;
	}
}
