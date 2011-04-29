#pragma once
#ifndef FLATMAP_HPP_TCWUAA47
#define FLATMAP_HPP_TCWUAA47

#include <new>
#include <stdlib.h>

namespace snow {
	template <typename K, typename V, bool KeepInsertionOrder = false>
	class FlatMap {
	public:
		class iterator;
		class const_iterator;
		
		typedef K key_type;
		typedef V data_type;
		
		FlatMap();
		FlatMap(const FlatMap<K,V,KeepInsertionOrder>& other);
		~FlatMap();
		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;
		iterator insert(const K& key, const V& value);
		size_t size() const { return _size; }
		bool empty() const { return size() == 0; }
		void reserve(size_t new_size);
		void resize(size_t new_size) { reserve(new_size); }
		void erase(const iterator& position);
		size_t erase(const K& key);
		void clear();
		void clear_no_resize();
		iterator find(const K& key);
		const_iterator find(const K& key) const;
		bool find_or_insert(iterator& iterator, const K& key, const V& value = V());
		V& operator[](const K& key);
		bool operator==(const FlatMap<K,V,KeepInsertionOrder>& other);
	private:
		const K* _keys;
		V* _values;
		size_t _size;
		size_t _alloc_size;
		
		struct KeyElement { byte _[sizeof(K)]; };
		struct ValueElement { byte _[sizeof(V)]; };
		
		bool find_index_or_insertion_point(const K& key, size_t& index_or_insertion_point) const; // returns true for index, false for insertion point
		void insert_at(size_t index, const K& key, const V& value);
	};
	
	template <typename K, typename V, bool KeepInsertionOrder>
	class FlatMap<K,V,KeepInsertionOrder>::iterator {
		iterator(const iterator& other) : _key(other._key), _value(other._value) {}
		iterator& operator++() { ++_key; ++_value; return *this; }
		iterator operator++(int) { iterator it(*this); ++_key; ++_value; return it; }
		FlatMap<K,V,KeepInsertionOrder>::iterator& operator*() { *this; }
		const FlatMap<K,V,KeepInsertionOrder>::iterator& operator*() const { *this; }
		FlatMap<K,V,KeepInsertionOrder>::iterator* operator->() { return this; }
		const FlatMap<K,V,KeepInsertionOrder>::iterator* operator->() const { return this; }
		const K& key() const { return *_key; }
		V& value() { return *_value; }
		const V& value() const { return *_value; }
		const K& first() const { return key(); }
		V& second() { return value(); }
		const V& second() const { return value(); }
	private:
		friend class FlatMap<K,V,KeepInsertionOrder>;
		friend class FlatMap<K,V,KeepInsertionOrder>::const_iterator;
		iterator(const K* key, const V* value) : _key(key), _value(value) {}
		const K* _key;
		V* _value;
	};
	
	template <typename K, typename V, bool KeepInsertionOrder>
	class FlatMap<K,V,KeepInsertionOrder>::const_iterator {
		const_iterator(const const_iterator& other) : _key(other._key), _value(other._value) {}
		const_iterator(const FlatMap<K,V,KeepInsertionOrder>::iterator& other) : _key(other._key), _value(other._value) {}
		const_iterator& operator++() { ++_key; ++_value; return *this; }
		const_iterator operator++(int) { const_iterator it(*this); ++_key; ++_value; return it; }
		const_iterator& operator*() { *this; }
		const const_iterator& operator*() const { *this; }
		const const_iterator* operator->() const { return this; }
		const K& key() const { return *_key; }
		const V& value() const { return *_value; }
		const K& first() const { return key(); }
		const V& second() const { return value(); }
	private:
		friend class FlatMap<K,V,KeepInsertionOrder>;
		const_iterator(const K* key, const V* value) : _key(key), _value(value) {}
		const K* _key;
		const V* _value;
	};
	
	template <typename K, typename V, bool KeepInsertionOrder>
	FlatMap<K,V,KeepInsertionOrder>::FlatMap() : _keys(NULL), _values(NULL), _size(0), _alloc_size(0) {}

	template <typename K, typename V, bool KeepInsertionOrder>
	FlatMap<K,V,KeepInsertionOrder>::FlatMap(const FlatMap<K,V,KeepInsertionOrder>& other) {
		_size = other._size;
		_alloc_size = other._size;
		_keys = (const K*)new KeyElement[_size];
		_values = (V*)new ValueElement[_size];
		for (size_t i = 0; i < _size; ++i) {
			// TODO: Use Move constructor
			new(_keys+i) K(other._keys[i]);
			new(_values+i) V(other._values[i]);
		}
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	FlatMap<K,V,KeepInsertionOrder>::~FlatMap() {
		for (size_t i = 0; i < _size; ++i) {
			_keys[i].~K();
			_values[i].~V();
		}
		delete[] (KeyElement*)_keys;
		delete[] (ValueElement*)_values;
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	typename FlatMap<K,V,KeepInsertionOrder>::iterator FlatMap<K,V,KeepInsertionOrder>::begin() {
		return iterator(_keys, _values);
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	typename FlatMap<K,V,KeepInsertionOrder>::iterator FlatMap<K,V,KeepInsertionOrder>::end() {
		return iterator(_keys+_size, _values+_size);
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	typename FlatMap<K,V,KeepInsertionOrder>::const_iterator FlatMap<K,V,KeepInsertionOrder>::begin() const {
		return const_iterator(_keys, _values);
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	typename FlatMap<K,V,KeepInsertionOrder>::const_iterator FlatMap<K,V,KeepInsertionOrder>::end() const {
		return const_iterator(_keys+_size, _values+_size);
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	bool FlatMap<K,V,KeepInsertionOrder>::find_or_insert(typename FlatMap<K,V,KeepInsertionOrder>::iterator& it, const K& key, const V& value) {
		size_t insertion_point;
		if (!find_index_or_insertion_point(key, insertion_point)) {
			insert_at(insertion_point, key, value);
			it = iterator(_keys+insertion_point, _values+insertion_point);
			return false;
		}
		it = iterator(_keys+insertion_point, _values+insertion_point);
		return true;
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	void FlatMap<K,V,KeepInsertionOrder>::reserve(size_t new_size) {
		if (_alloc_size < new_size) {
			// TODO: Use Move constructor
			K* new_keys = (K*)new KeyElement[new_size];
			V* new_values = (V*)new ValueElement[new_size];
			for (size_t i = 0; i < _size; ++i) {
				new(new_keys+i) K(_keys[i]);
				_keys[i].~K();
				new(new_values+i) V(_values[i]);
				_values[i].~V();
			}
			delete[] (KeyElement*)_keys;
			delete[] (ValueElement*)_values;
			_keys = new_keys;
			_values = new_values;
			_alloc_size = new_size;
		}
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	void FlatMap<K,V,KeepInsertionOrder>::clear() {
		clear_no_resize();
		delete[] (KeyElement*)_keys;
		delete[] (ValueElement*)_values;
		_keys = NULL;
		_values = NULL;
		_alloc_size = 0;
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	void FlatMap<K,V,KeepInsertionOrder>::clear_no_resize() {
		for (size_t i = 0; i < _size; ++i) {
			_keys[i].~K();
			_values[i].~V();
		}
		_size = 0;
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	typename FlatMap<K,V,KeepInsertionOrder>::iterator FlatMap<K,V,KeepInsertionOrder>::find(const K& key) {
		size_t idx;
		if (find_index_or_insertion_point(key, idx)) {
			return iterator(_keys+idx, _values+idx);
		}
		return end();
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	typename FlatMap<K,V,KeepInsertionOrder>::const_iterator FlatMap<K,V,KeepInsertionOrder>::find(const K& key) const {
		size_t idx;
		if (find_index_or_insertion_point(key, idx)) {
			return const_iterator(_keys+idx, _values+idx);
		}
		return end();
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	V& FlatMap<K,V,KeepInsertionOrder>::operator[](const K& key) {
		size_t idx;
		if (!find_index_or_insertion_point(key, idx)) {
			insert_at(idx, key, V());
		}
		return _values[idx];
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	bool FlatMap<K,V,KeepInsertionOrder>::operator==(const FlatMap<K,V,KeepInsertionOrder>& other) {
		if (_size != other._size) return false;
		for (size_t i = 0; i < _size; ++i) {
			if (_keys[i] != other._keys[i]) return false;
			if (_values[i] != other._values[i]) return false;
		}
		return true;
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	bool FlatMap<K,V,KeepInsertionOrder>::find_index_or_insertion_point(const K& key, size_t& idx) const {
		if (KeepInsertionOrder) {
			// Linear search
			for (size_t i = 0; i < _size; ++i) {
				if (_keys[i] == key) {
					idx = i;
					return true;
				}
			}
			idx = _size;
			return false;
		} else {
			// Binary search
			size_t min = 0;
			size_t max = _size;
			size_t mid = max / 2;
			while (min <= max && _keys[mid] != key) {
				mid = min + (max - min) / 2;
				if (_compare(key, _keys[mid])) {
					max = mid;
				} else {
					min = mid;
				}
			}
			idx = mid;
			if (_keys[mid] == key) return true;
			return false;
		}
	}
	
	template <typename K, typename V, bool KeepInsertionOrder>
	void FlatMap<K,V,KeepInsertionOrder>::insert_at(size_t pos, const K& key, const V& value) {
		reserve(pos+1);
		// shift everyone right
		size_t to_move = _size - pos;
		for (size_t i = 0; i < to_move; ++i) {
			size_t target = _size-i;
			size_t source = _size-i-1;
			// TODO: Use Move constructor
			new(_keys+target) K(_keys[source]);
			_keys[source].~K();
			new(_values+target) V(_values[source]);
			_values[source].~V();
		}
		new(_keys+pos) K(key);
		new(_values+pos) V(value);
		++_size;
	}
}

#endif /* end of include guard: FLATMAP_HPP_TCWUAA47 */
