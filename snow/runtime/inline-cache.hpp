#pragma once
#ifndef INLINE_CACHE_HPP_W7H8DR6Y
#define INLINE_CACHE_HPP_W7H8DR6Y

#include "snow/function.hpp"
#include "snow/class.hpp"
#include "snow/snow.hpp"

namespace snow {
	enum CacheState {
		CacheStateUninitialized,
		CacheStatePremorphic,
		CacheStateMonomorphic
	};
	
	struct MethodCacheLine {
		CacheState state;
		VALUE cls;
		MethodQueryResult method;
		MethodCacheLine() : state(CacheStateUninitialized), cls(NULL), method((MethodQueryResult){MethodTypeNone, NULL}) {}
	};
	
	struct InstanceVariableCacheLine {
		CacheState state;
		VALUE cls;
		int32_t index_of_instance_variable;
		InstanceVariableCacheLine() : state(CacheStateUninitialized), cls(NULL), index_of_instance_variable(-1) {}
	};
	
	inline void get_method_inline_cache(VALUE val, Symbol name, MethodCacheLine* cache, MethodQueryResult* out_method) {
		VALUE cls = get_class(val);
		switch (cache->state) {
			case CacheStateUninitialized: {
				cache->state = CacheStatePremorphic;
				cache->cls = cls;
				class_lookup_method(cls, name, out_method);
				break;
			}
			case CacheStatePremorphic: {
				if (cache->cls == cls) {
					cache->state = CacheStateMonomorphic;
				}
				cache->cls = cls;
				class_lookup_method(cls, name, &cache->method);
				*out_method = cache->method;
				break;
			}
			case CacheStateMonomorphic: {
				if (cache->cls == cls) {
					// cache hit!
					*out_method = cache->method;
				} else {
					class_lookup_method(cls, name, out_method);
				}
				break;
			}
		}
	}
	
	inline int32_t get_instance_variable_inline_cache(VALUE val, Symbol name, InstanceVariableCacheLine* cache) {
		VALUE cls = get_class(val);
		switch (cache->state) {
			case CacheStateUninitialized: {
				cache->state = CacheStatePremorphic;
				cache->cls = cls;
				return class_get_index_of_instance_variable(cls, name);
			}
			case CacheStatePremorphic: {
				if (cache->cls == cls) {
					cache->state = CacheStateMonomorphic;
				}
				cache->cls = cls;
				cache->index_of_instance_variable = class_get_index_of_instance_variable(cls, name);
				return cache->index_of_instance_variable;
			}
			case CacheStateMonomorphic: {
				if (cache->cls == cls) {
					// cache hit!
					return cache->index_of_instance_variable;
				} else {
					return class_get_index_of_instance_variable(cls, name);
				}
			}
		}
	}
	
	inline int32_t get_or_define_instance_variable_inline_cache(VALUE val, Symbol name, InstanceVariableCacheLine* cache) {
		VALUE cls = get_class(val);
		switch(cache->state) {
			case CacheStateUninitialized: {
				cache->state = CacheStatePremorphic;
				cache->cls = cls;
				return object_get_or_create_index_of_instance_variable(val, name);
			}
			case CacheStatePremorphic: {
				if (cache->cls == cls) {
					cache->state = CacheStateMonomorphic;
				}
				cache->cls = cls;
				cache->index_of_instance_variable = object_get_or_create_index_of_instance_variable(val, name);
				return cache->index_of_instance_variable;
			}
			case CacheStateMonomorphic: {
				if (cache->cls == cls) {
					// cache hit!
					return cache->index_of_instance_variable;
				} else {
					return object_get_or_create_index_of_instance_variable(val, name);
				}
			}
		}
	}
}

#endif /* end of include guard: INLINE_CACHE_HPP_W7H8DR6Y */
