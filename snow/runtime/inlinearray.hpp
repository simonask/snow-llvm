#pragma once
#ifndef INLINEARRAY_HPP_V1R4E6UD
#define INLINEARRAY_HPP_V1R4E6UD

#include "snow/util.hpp"

namespace snow {
	template <size_t NUM_INLINED, typename T>
	class InlineArray {
		InlineArray();
		template <size_t OTHER_NUM_INLINED>
		InlineArray(const InlineArray<OTHER_NUM_INLINED, T>& other);
		~InlineArray();
		size_t size() const                        { return _size; }
		size_t allocated_size() const              { return _alloc_size; }
		void reserve(size_t sz);
		void resize(size_t sz, const T& x = T());
		void clear(bool free_heap_memory = true);
		T& push(const T& x = T());
		void pop();
		T& back()                                  { return (*this)[size()-1]; }
		void erase(size_t idx);
		T& operator[](size_t idx);
		const T& operator[](size_t idx) const;
		T& get_or_create_element_at_index(size_t idx, const T& default_value = T());
	private:
		typedef Placeholder<T> Element;
		uint32_t _size;
		uint32_t _alloc_size;
		Element* _data;
		Element _inlined[NUM_INLINED];
	};
	
	template <size_t NUM_INLINED, typename T>
	InlineArray<NUM_INLINED,T>::InlineArray() : _size(0), _alloc_size(NUM_INLINED), _data(NULL) {}
	
	template <size_t NUM_INLINED, typename T>
	template <size_t OTHER_NUM_INLINED>
	InlineArray<NUM_INLINED,T>::InlineArray(const InlineArray<OTHER_NUM_INLINED, T>& other) : _size(0), _alloc_size(NUM_INLINED), _data(NULL) {
		reserve(other.size());
		for (size_t i = 0; i < other.size(); ++i) {
			push(other[i]);
		}
	}
	
	template <size_t NUM_INLINED, typename T>
	InlineArray<NUM_INLINED,T>::~InlineArray() {
		clear(true);
	}
	
	template <size_t NUM_INLINED, typename T>
	void InlineArray<NUM_INLINED,T>::reserve(size_t sz) {
		if (sz > _alloc_size) {
			ssize_t heap_alloc_size = sz - NUM_INLINED;
			ASSERT(heap_alloc_size > 0); // class invariant: _alloc_size >= NUM_INLINED
			Element* new_data = alloc_range<Element>(heap_alloc_size);
			ssize_t num_heap_constructed = _size - NUM_INLINED;
			if (num_heap_constructed > 0) {
				copy_construct<T>((T*)new_data, (T*)_data, num_heap_constructed);
			}
			_alloc_size = sz;
		}
	}
	
	template <size_t NUM_INLINED, typename T>
	void InlineArray<NUM_INLINED,T>::resize(size_t sz, const T& x) {
		reserve(sz);
		while (size() < sz) {
			push(x);
		}
	}
	
	template <size_t NUM_INLINED, typename T>
	void InlineArray<NUM_INLINED,T>::clear(bool free_heap_memory) {
		size_t num_inlined = _size > NUM_INLINED ? NUM_INLINED : _size;
		destruct_range((T*)_inlined, num_inlined);
		ssize_t num_heap_constructed = _size - num_inlined;
		if (num_heap_constructed) {
			destruct_range((T*)_data, num_heap_constructed);
		}
		_size = 0;
		if (free_heap_memory) {
			dealloc_range(_data, 0);
			_data = NULL;
		}
	}
	
	template <size_t NUM_INLINED, typename T>
	T& InlineArray<NUM_INLINED,T>::push(const T& x) {
		return get_or_create_element_at_index(size(), x);
	}
	
	template <size_t NUM_INLINED, typename T>
	void InlineArray<NUM_INLINED,T>::pop() {
		if (size())
			erase(size()-1)
	}
	
	template <size_t NUM_INLINED, typename T>
	T& InlineArray<NUM_INLINED,T>::operator[](size_t idx) {
		ASSERT(idx < size());
		if (idx < NUM_INLINED) {
			return *reinterpret_cast<T*>(_inlined + idx);
		} else {
			return *reinterpret_cast<T*>(_data + (idx - NUM_INLINED));
		}
	}
	
	template <size_t NUM_INLINED, typename T>
	const T& InlineArray<NUM_INLINED,T>::operator[](size_t idx) {
		ASSERT(idx < size()); // index out of bounds
		if (idx < NUM_INLINED) {
			return *reinterpret_cast<const T*>(_inlined + idx);
		} else {
			return *reinterpret_cast<const T*>(_data + (idx - NUM_INLINED));
		}
	}
	
	template <size_t NUM_INLINED, typename T>
	T& InlineArray<NUM_INLINED,T>::get_or_create_element_at_index(size_t idx, const T& default_value) {
		resize(idx+1, default_value);
		return (*this)[idx];
	}
}

#endif /* end of include guard: INLINEARRAY_HPP_V1R4E6UD */
