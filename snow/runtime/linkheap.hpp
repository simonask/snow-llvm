#pragma once
#ifndef LINKHEAP_HPP_6DSWUCB4
#define LINKHEAP_HPP_6DSWUCB4

#include "snow/basic.h"

#include <new>

namespace snow {
	template <typename T>
	class LinkHeap {
	public:
		LinkHeap();
		~LinkHeap();
		T* alloc();
		void free(T*);
		void clear();
		
		// features used by the GC -- use with caution!
		bool contains(const void*) const;
		int64_t index_of(const void*) const;
		T* object_at_index(int64_t idx) const;
		int64_t max_num_objects() const;
		const void* get_first_free() const;
		const void* get_next_free(const void* x) const;
		void get_min_max(intptr_t& out_min, intptr_t& out_max) const;
	private:
		LinkHeap(const LinkHeap<T>&) {} // hide copy constructor
		struct Page;
		struct PageInfo {
			Page* next;
			size_t offset;
		protected: PageInfo() {}
		};

		struct Element {
			union {
				Element* next_free;
				byte data[sizeof(T)];
			};
		};

		static const size_t ELEMENTS_PER_PAGE = (SN_MEMORY_PAGE_SIZE - SN_MALLOC_OVERHEAD - sizeof(PageInfo)) / sizeof(Element);

		struct Page : public PageInfo {
			Element data[ELEMENTS_PER_PAGE];
		};
		
		Page* _head;
		Page* _tail;
		Element* _free;
	};
	
	template <typename T>
	LinkHeap<T>::LinkHeap() : _head(NULL), _tail(NULL), _free(NULL) {}
	
	template <typename T>
	LinkHeap<T>::~LinkHeap() {
		clear();
	}
	
	template <typename T>
	T* LinkHeap<T>::alloc() {
		T* x;
		if (_free) {
			Element* next_free = _free->next_free;
			x = new(_free) T;
			_free = next_free;
		} else {
			if (!_tail || _tail->offset == ELEMENTS_PER_PAGE) {
				Page* old_tail = _tail;
				_tail = new Page;
				_tail->next = NULL;
				_tail->offset = 0;
				if (!_head)
					_head = _tail;
				if (old_tail)
					old_tail->next = _tail;
			}
			
			x = new(_tail->data + (_tail->offset++)) T;
		}
		return x;
	}
	
	template <typename T>
	void LinkHeap<T>::free(T* x) {
		if (!x) return;
		x->~T();
		Element* e = reinterpret_cast<Element*>(x);
		e->next_free = _free;
		_free = e;
	}
	
	template <typename T>
	void LinkHeap<T>::clear() {
		Page* p = _head;
		while (p) {
			Page* n = p->next;
			delete p;
			p = n;
		}
	}
	
	template <typename T>
	bool LinkHeap<T>::contains(const void* _ptr) const {
		return index_of(_ptr) >= 0;
	}
	
	template <typename T>
	int64_t LinkHeap<T>::index_of(const void* _ptr) const {
		size_t page_index = 0;
		Element* x = (Element*)_ptr;
		Page* p = _head;
		while (p) {
			Element* base = p->data;
			Element* end = base + p->offset;
			if (x >= base && x < end) {
				return page_index*ELEMENTS_PER_PAGE + (x - base);
			}
			p = p->next;
			++page_index;
		}
		return -1;
	}
	
	template <typename T>
	T* LinkHeap<T>::object_at_index(int64_t idx) const {
		int64_t page_index = idx / ELEMENTS_PER_PAGE;
		int64_t object_index = idx % ELEMENTS_PER_PAGE;
		Page* p = _head;
		size_t pi = 0;
		while (p) {
			if (pi == page_index) {
				if (object_index >= p->offset) {
					return NULL;
				}
				return (T*)(p->data + object_index);
			}
			p = p->next;
			++page_index;
		}
		return NULL;
	}
	
	template <typename T>
	int64_t LinkHeap<T>::max_num_objects() const {
		int64_t object_count = 0;
		Page* p = _head;
		while (p) {
			if (p->next) object_count += ELEMENTS_PER_PAGE;
			else object_count += p->offset;
			p = p->next;
		}
		return object_count;
	}
	
	template <typename T>
	const void* LinkHeap<T>::get_first_free() const {
		return _free;
	}
	
	template <typename T>
	const void* LinkHeap<T>::get_next_free(const void* _x) const {
		ASSERT(_x);
		Element* x = (Element*)_x;
		return x->next_free;
	}
	
	template <typename T>
	void LinkHeap<T>::get_min_max(intptr_t& min, intptr_t& max) const {
		min = INTPTR_MAX;
		max = NULL;
		
		Page* p = _head;
		while (p) {
			intptr_t begin = (intptr_t)p->data;
			intptr_t end = (intptr_t)(p->data + p->offset);
			if (begin < min) min = begin;
			if (end > max) max = end;
			p = p->next;
		}
	}
}

#endif /* end of include guard: LINKHEAP_HPP_6DSWUCB4 */
