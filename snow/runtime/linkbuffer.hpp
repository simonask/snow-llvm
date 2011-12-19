#pragma once
#ifndef LINKBUFFER_HPP_SYOSTGNZ
#define LINKBUFFER_HPP_SYOSTGNZ

#include "snow/basic.h"

#include <new>

namespace snow {
	template <typename T>
	class LinkBuffer {
	public:
		LinkBuffer() : _head(NULL), _tail(NULL) {}
		~LinkBuffer() { clear(); }
		size_t size() const;
		void clear();
		T& push(const T& = T());
		void push_range(const T* begin, const T* end);
		size_t extract(T* destination, size_t n) const;
		void modify(size_t offset, size_t len, const T* new_data);
		
		class iterator;
		iterator begin();
		iterator end();
	private:
		struct Page;
		struct PageInfo {
			Page* next;
			size_t offset;
			size_t page_index;
		protected: PageInfo() {}
		};

		struct Element {
			byte x[sizeof(T)];
		};

		static const size_t ELEMENTS_PER_PAGE = (SN_MEMORY_PAGE_SIZE - sizeof(PageInfo)) / sizeof(Element);

		struct Page : public PageInfo {
			Element data[ELEMENTS_PER_PAGE];
		};
		friend class iterator;
		Page* _head;
		Page* _tail;
	};
	
	template <typename T>
	class LinkBuffer<T>::iterator {
	public:
		iterator() : _page(NULL), _offset(0) {}
		iterator& operator++() { increment(); return *this; }
		iterator operator++(int) { iterator it = *this; increment(); return it; }
		iterator operator+(int x) { iterator it = *this; for (int i = 0; i < x; ++i) it.increment(); return it; }
		T* operator->() const { return reinterpret_cast<T*>(_page->data + _offset); }
		T& operator*() const { return *reinterpret_cast<T*>(_page->data + _offset); }
		iterator(const LinkBuffer<T>::iterator& other) : _page(other._page), _offset(other._offset) {}
		iterator& operator=(const LinkBuffer<T>::iterator& other) { _page = other._page; _offset = other._offset; return *this; }
		bool operator==(const LinkBuffer<T>::iterator& other) const { return _page==other._page && _offset==other._offset; }
		bool operator!=(const LinkBuffer<T>::iterator& other) const { return !(*this == other); }
		bool at_end() const { return _page == NULL && _offset == 0; }
		size_t index() const { return _page ? (_page->page_index * LinkBuffer<T>::ELEMENTS_PER_PAGE + _offset) : 0; }
		bool operator>(const LinkBuffer<T>::iterator& other) const { return index() > other.index(); }
		bool operator<(const LinkBuffer<T>::iterator& other) const { return index() < other.index(); }
		bool operator>=(const LinkBuffer<T>::iterator& other) const { return index() >= other.index(); }
		bool operator<=(const LinkBuffer<T>::iterator& other) const { return index() <= other.index(); }
	private:
		friend class LinkBuffer<T>;
		
		iterator(LinkBuffer<T>::Page* p) : _page(p), _offset(0) {}
		void increment() {
			if (_offset+1 < _page->offset) ++_offset;
			else { _page = _page->next; _offset = 0; }
		}
		LinkBuffer<T>::Page* _page;
		size_t _offset;
	};
	
	
	// --------------------------------------------------------------------
	
	template <typename T>
	size_t LinkBuffer<T>::size() const {
		size_t size = 0;
		Page* page = _head;
		while (page) {
			if (page->next)
				size += ELEMENTS_PER_PAGE;
			else
				size += page->offset;
			page = page->next;
		}
		return size;
	}
	
	template <typename T>
	void LinkBuffer<T>::clear() {
		Page* page = _head;
		while (page) {
			// call destructors
			for (size_t i = 0; i < page->offset; ++i) {
				reinterpret_cast<T*>(page->data + i)->~T();
			}
			Page* next = page->next;
			delete page;
			page = next;
		}
		_head = NULL;
		_tail = NULL;
	}
	
	template <typename T>
	T& LinkBuffer<T>::push(const T& x) {
		if (!_tail || _tail->offset == ELEMENTS_PER_PAGE) {
			Page* old_tail = _tail;
			_tail = new Page;
			_tail->next = NULL;
			_tail->offset = 0;
			_tail->page_index = old_tail ? old_tail->page_index+1 : 0;
			if (!_head)
				_head = _tail;
			if (old_tail)
				old_tail->next = _tail;
		}
		
		T* e = reinterpret_cast<T*>(_tail->data + (_tail->offset++));
		// copy-construct
		new(e) T(x);
		return *e;
	}
	
	template <typename T>
	void LinkBuffer<T>::push_range(const T* begin, const T* end) {
		// TODO: optimize
		ASSERT(begin < end);
		for (const T* p = begin; p < end; ++p) {
			push(*p);
		}
	}
	
	template <typename T>
	size_t LinkBuffer<T>::extract(T* destination, size_t n) const {
		Page* current = _head;
		size_t copied = 0;
		while (current && copied < n) {
			size_t remaining = n - copied; // remaining space in dst
			size_t to_copy = remaining < current->offset ? remaining : current->offset;
			for (size_t i = 0; i < to_copy; ++i) {
				destination[copied + i] = *reinterpret_cast<const T*>(current->data + i);
			}
			copied += to_copy;
			current = current->next;
		}
		return copied;
	}
	
	template <typename T>
	typename LinkBuffer<T>::iterator LinkBuffer<T>::begin() {
		return iterator(_head);
	}
	
	template <typename T>
	typename LinkBuffer<T>::iterator LinkBuffer<T>::end() {
		return iterator();
	}
}

#endif /* end of include guard: LINKBUFFER_HPP_SYOSTGNZ */
