#pragma once
#ifndef TYPE_H_ZR7ZWP35
#define TYPE_H_ZR7ZWP35

#include "snow/basic.h"
#include "snow/value.hpp"

namespace snow {
	typedef void(*GCCallback)(VALUE);
	
	struct Type {
		size_t data_size;
		void(*initialize)(void* data);
		void(*finalize)(void* data);
		void(*copy)(void* data, void* other_data);
		void(*gc_each_root)(void* data, GCCallback callback);
	};
	inline void dummy_gc_each_root_callback(void* data, GCCallback callback) {}
	
	template <typename T> struct TypeRegistry;
	template <typename T> struct TypeRegistry<const T> { static const Type* get() { return TypeRegistry<T>::get(); } };
	template <typename T> struct TypeRegistry { static const Type* get(); };
	template <typename T> const Type* get_type() { return TypeRegistry<T>::get(); }
}

#define SN_REGISTER_TYPE(T, SPEC) template <> const snow::Type* snow::TypeRegistry<T>::get() { static snow::Type t = SPEC; return &t; }
#define SN_REGISTER_CPP_TYPE(T, GC_EACH_ROOT_FUNC) SN_REGISTER_TYPE(T, ((snow::Type){ .data_size = sizeof(T), .initialize = snow::construct<T>, .finalize = snow::destruct<T>, .copy = snow::assign<T>, .gc_each_root = GC_EACH_ROOT_FUNC }))
#define SN_REGISTER_SIMPLE_CPP_TYPE(T) SN_REGISTER_CPP_TYPE(T, snow::dummy_gc_each_root_callback)

#endif /* end of include guard: TYPE_H_ZR7ZWP35 */
