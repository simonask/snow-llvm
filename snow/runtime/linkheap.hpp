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
}

#endif /* end of include guard: LINKHEAP_HPP_6DSWUCB4 */
