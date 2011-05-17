#pragma once
#ifndef UTIL_HPP_A83WWPWN
#define UTIL_HPP_A83WWPWN

#include "snow/basic.h"
#include <new>

namespace snow {
	template <typename T>
	struct Placeholder { byte _[sizeof(T)]; };
	
	template <typename A, typename B>
	inline ssize_t compare(const A& a, const B& b) {
		return a - b;
	}
	
	template <typename T>
	inline void construct_range(T* range, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			new(range+i) T();
		}
	}
	
	template <typename T, typename U>
	inline void construct_range(T* range, size_t num, U& val) {
		for (size_t i = 0; i < num; ++i) {
			new(range+i) T(val);
		}
	}
	
	template <typename T>
	inline void destruct_range(T* range, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			range[i].~T();
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
	inline void dealloc_range(T* range, size_t num_constructed = 0) {
		for (size_t i = 0; i < num_constructed; ++i) {
			range[i].~T();
		}
		delete[] reinterpret_cast<Placeholder<T>*>(range);
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
	
	template <typename C, typename T>
	inline ssize_t index_of(const C& container, const T& element) {
		ssize_t idx = 0;
		for (typename C::const_iterator it = container.begin(); it != container.end(); ++it) {
			if (*it == element) return idx;
			++idx;
		}
		return -1;
	}
}

#endif /* end of include guard: UTIL_HPP_A83WWPWN */
