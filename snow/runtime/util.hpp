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
	inline void copy_range(T* dst, const T* src, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			dst[i] = src[i];
		}
	}
	
	template <typename T>
	inline T* insert_in_range(T* begin, T* end, T* insert_before, const T& element) {
		ASSERT(begin < end);
		ASSERT(begin <= insert_before);
		const size_t old_size = end - begin;
		const size_t new_size = old_size + 1;
		const size_t elements_before = insert_before - begin;
		const size_t elements_after = end - insert_before;
		T* new_range = alloc_range<T>(new_size);
		// TODO: Use C++1x Move
		copy_construct_range(new_range, begin, elements_before);
		new(new_range+elements_before) T(element);
		copy_construct_range(new_range + elements_before + 1, begin + elements_before, elements_after);
		dealloc_range(begin, old_size);
		return new_range;
	}
	
	template <typename I, typename T, typename Compare, bool LowerBound>
	inline const I binary_search_impl(const I begin, const I end, const T& key, Compare compare) {
		I min = begin;
		I max = end;
		I mid = begin + ((end - begin) / 2);
		while (compare(*min, *max) <= 0 && compare(key, *mid) != 0) {
			mid = min + (max - min) / 2;
			if (compare(key, *mid) < 0) {
				max = mid;
			} else {
				min = mid;
			}
		}
		if (LowerBound) {
			return mid;
		} else {
			if (compare(key, *mid) == 0) return mid;
			return end;
		}
	}
	
	template <typename I, typename T, typename Compare>
	inline const I binary_search(const I begin, const I end, const T& key, Compare compare) {
		return binary_search_impl<I,T,Compare,false>(begin, end, key, compare);
	}
	
	template <typename I, typename T, typename Compare>
	inline const I binary_search_lower_bound(const I begin, const I end, const T& key, Compare compare) {
		return binary_search_impl<I,T,Compare,true>(begin, end, key, compare);
	}
}

#endif /* end of include guard: UTIL_HPP_A83WWPWN */
