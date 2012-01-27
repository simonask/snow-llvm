#pragma once
#ifndef UTIL_HPP_A83WWPWN
#define UTIL_HPP_A83WWPWN

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/gc.hpp"
#include <new>

namespace snow {
	template <typename T>
	struct Placeholder : GCAlloc { byte _[sizeof(T)]; };
	
	template <typename A, typename B> struct ReplicateConstPtr;
	template <typename A, typename B> struct ReplicateConstPtr<const A, B> { typedef const B* Result; };
	template <typename A, typename B> struct ReplicateConstPtr { typedef B* Result; };
	
	template <typename A> struct RemoveConst;
	template <typename A> struct RemoveConst<const A> { typedef A Result; };
	template <typename A> struct RemoveConst { typedef A Result; };
	
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
		typedef typename ReplicateConstPtr<T, Placeholder<T> >::Result PlaceholderPtr;
		for (size_t i = 0; i < num_constructed; ++i) {
			range[i].~T();
		}
		delete[] reinterpret_cast<PlaceholderPtr>(range);
		range = NULL;
	}
	
	template <typename T>
	inline T* realloc_range(T* old, size_t old_size, size_t new_size) {
		T* ptr = alloc_range<T>(new_size);
		copy_construct_range(ptr, old, old_size);
		dealloc_range(old, old_size);
		return ptr;
	}
	
	template <typename T, typename U>
	inline void copy_range(T* dst, const U* src, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			dst[i] = src[i];
		}
	}
	
	template <typename T, typename U>
	inline void assign_range(T* dst, const U& x, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			dst[i] = x;
		}
	}
	
	template <typename T>
	inline void move_range(T* dst, T* src, size_t num) {
		if (dst < src) {
			for (size_t i = 0; i < num; ++i) {
				dst[i] = std::move(src[i]);
			}
		} else if (dst > src) {
			for (size_t i = 0; i < num; ++i) {
				dst[num-i-1] = std::move(src[num-i-1]);
			}
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
	
	template <typename T>
	class StackArray {
		T* data_;
		size_t size_;
	public:
		StackArray(byte* data, size_t size) : data_(reinterpret_cast<T*>(data)), size_(size) { construct_range(data_, size_); }
		~StackArray() { destruct_range(data_, size_); }
		T& operator[](size_t idx) const { ASSERT(idx < size_); return data_[idx]; }
		size_t size() const { return data_; }
		operator T*() const { return data_; }
	};
	#define SN_STACK_ARRAY(T, NAME, SIZE) byte stack_array_data__ ## __LINE__[SIZE * sizeof(T)]; snow::StackArray<T> NAME(stack_array_data__ ## __LINE__, SIZE)
	
	struct NonNewable {
	private:
		void* operator new(size_t sz);
		void* operator new[](size_t sz);
		void operator delete(void*);
		void operator delete[](void*);
	};
	
	template <bool COND> struct StaticAssertion;
	template <> struct StaticAssertion<true> { static const int RESULT = 1; };
	#define SN_STATIC_ASSERT(COND) enum { _SN_STATIC_ASSERTION__ ## __LINE__ = snow::StaticAssertion<(bool)(COND)>::RESULT };
}

#endif /* end of include guard: UTIL_HPP_A83WWPWN */
