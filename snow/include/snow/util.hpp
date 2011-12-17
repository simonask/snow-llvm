#pragma once
#ifndef UTIL_HPP_A83WWPWN
#define UTIL_HPP_A83WWPWN

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/object.hpp"
#include <new>

namespace snow {
	template <typename T>
	struct Placeholder { byte _[sizeof(T)]; };
	
	template <typename A, typename B>
	inline ssize_t compare(const A& a, const B& b) {
		return a - b;
	}
	
	template <typename T>
	inline void construct(void* element) {
		new(element) T();
	}
	
	template <typename T, typename U>
	inline void copy_construct(void* element, const U& other) {
		new(element) T(other);
	}
	
	template <typename T, typename U>
	inline void copy_construct(void* element, U& other) {
		new(element) T(other);
	}
	
	template <typename T>
	inline void destruct(void* element) {
		reinterpret_cast<T*>(element)->~T();
	}
	
	template <typename T>
	inline void assign(void* a, void* b) {
		*reinterpret_cast<T*>(a) = *reinterpret_cast<T*>(b);
	}
	
	template <typename T>
	inline void construct_range(T* range, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			construct<T>(range+i);
		}
	}
	
	template <typename T, typename U>
	inline void construct_range(T* range, size_t num, U& val) {
		for (size_t i = 0; i < num; ++i) {
			copy_construct<T,U>(range+i, val);
		}
	}
	
	template <typename T>
	inline void destruct_range(T* range, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			destruct<T>(range+i);
		}
	}
	
	template <typename T>
	inline void copy_construct_range(T* dst, const T* src, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			new(dst+i) T(src[i]);
		}
	}
	
	// TODO: Add move_construct_range
	
	template <typename T>
	inline T* alloc_range(size_t num) {
		return reinterpret_cast<T*>(new Placeholder<T>[num]);
	}
	
	template <typename T>
	inline void dealloc_range(T*& range, size_t num_constructed = 0) {
		for (size_t i = 0; i < num_constructed; ++i) {
			range[i].~T();
		}
		delete[] reinterpret_cast<Placeholder<T>*>(range);
		range = NULL;
	}
	
	template <typename T>
	inline T* realloc_range(T* old, size_t old_size, size_t new_size) {
		T* ptr = alloc_range<T>(new_size);
		copy_construct_range(ptr, old, old_size);
		dealloc_range(old, old_size);
		return ptr;
	}
	
	template <typename T>
	inline void copy_range(T* dst, const T* src, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			dst[i] = src[i];
		}
	}
	
	template <typename T>
	inline void assign_range(T* dst, const T& x, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			dst[i] = x;
		}
	}
	
	template <typename T>
	inline T* duplicate_range(const T* src, size_t num) {
		if (num == 0) return NULL;
		T* p = alloc_range<T>(num);
		copy_construct_range(p, src, num);
		return p;
	}
	
	template <typename C, typename T>
	inline ssize_t index_of(const C& container, const T& element) {
		ssize_t idx = 0;
		for (typename C::const_iterator it = container.begin(); it != container.end(); ++it) {
			if (*it == element) return idx;
			++idx;
		}
		return -1;
	}
	
	struct NonNewable {
	private:
		void* operator new(size_t sz);
		void* operator new[](size_t sz);
		void operator delete(void*);
		void operator delete[](void*);
	};
	
	template <bool COND> struct StaticAssertion;
	template <> struct StaticAssertion<true> {};
	#define SN_STATIC_ASSERT(COND) enum { dummy = snow::StaticAssertion<(bool)(COND)> }
	
	template <typename A, typename B> struct ReplicateConstPtr;
	template <typename A, typename B> struct ReplicateConstPtr<const A, B> { typedef const B* Result; };
	template <typename A, typename B> struct ReplicateConstPtr { typedef B* Result; };
	
	template <typename T>
	T* object_get_private(SnObject* obj, const Type* check_type) {
		return reinterpret_cast<T*>(snow_object_get_private(obj, check_type));
	}
	
	template <typename T>
	const T* object_get_private(const SnObject* obj, const Type* check_type) {
		return reinterpret_cast<const T*>(snow_object_get_private(obj, check_type));
	}
	
	template <typename T>
	T* value_get_private(VALUE val, const Type* check_type) {
		if (snow_is_object(val)) {
			return object_get_private<T>((SnObject*)val, check_type);
		}
		return NULL;
	}
	
	inline bool object_is_of_type(const SnObject* obj, const Type* check_type) {
		return snow_object_is_of_type(obj, check_type);
	}
	
	inline bool value_is_of_type(VALUE val, const Type* check_type) {
		if (snow_is_object(val)) {
			return object_is_of_type((SnObject*)val, check_type);
		}
		return false;
	}
	
	inline bool value_is_of_type(VALUE val, SnValueType check_type) {
		return snow_type_of(val) == check_type;
	}
}

#endif /* end of include guard: UTIL_HPP_A83WWPWN */
