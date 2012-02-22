#pragma once
#ifndef ARGUMENTS_H_5M3JAXPP
#define ARGUMENTS_H_5M3JAXPP

#include "snow/value.hpp"
#include "snow/symbol.hpp"

namespace snow {
	/*
		This class is an abstraction of this concept: A pointer to an array, that may optionally at some
		point become autonomous and manage its own memory.
	*/
	template <typename T>
	class ArrayOrRef {
	public:
		typedef T ValueType;
		typedef typename RemoveConst<T>::Result NonConstValueType;
		typedef const NonConstValueType ConstValueType;
		typedef ValueType* Iterator;
		typedef ConstValueType* ConstIterator;
		// STL compatibility:
		typedef ValueType value_type;
		typedef Iterator iterator;
		typedef ConstIterator const_iterator;
		
		ArrayOrRef() : data_(NULL), size_(0) {}
		template <size_t N>
		ArrayOrRef(const T data[N]) : data_(data), size_(N), owns_data_(false) {}
		ArrayOrRef(const T* data, size_t sz) : data_(data), size_(sz), owns_data_(false) {}
		ArrayOrRef(const ArrayOrRef<T>& other) : data_(other.data_), size_(other.size_), owns_data_(false) {}
		ArrayOrRef(ArrayOrRef<T>&& other) : data_(other.data_), size_(other.size_), owns_data_(other.owns_data_) {
			other.owns_data_ = false;
		}
		ArrayOrRef<T>& operator=(const ArrayOrRef<T>& other) {
			clear();
			data_ = other.data_;
			size_ = other.size_;
			owns_data_ = false;
			return *this;
		}
		ArrayOrRef<T>& operator=(ArrayOrRef<T>&& other) {
			clear();
			data_ = other.data_;
			size_ = other.size_;
			owns_data_ = other.owns_data_;
			other.owns_data_ = false;
			return *this;
		}
		~ArrayOrRef() {
			clear();
		}
		
		void take_ownership() {
			if (!owns_data_ && data_ != nullptr) {
				NonConstValueType* data = snow::alloc_range<NonConstValueType>(size_);
				snow::copy_range(data, data_, size_);
				data_ = data;
				owns_data_ = true;
			}
		}
		
		void clear() {
			if (owns_data_) {
				snow::dealloc_range(data_, size_);
			}
			data_ = nullptr;
			size_ = 0;
			owns_data_ = false;
		}
		
		ConstValueType& operator[](size_t idx) const {
			ASSERT(idx < size_);
			return data_[idx];
		}
		ValueType& operator[](size_t idx) {
			ASSERT(idx < size_);
			return data_[idx];
		}
		size_t size() const { return size_; }
		
		bool has_ownership() const { return owns_data_; }
		
		Iterator begin() { return data_; }
		Iterator end() { return data_ + size_; }
		ConstIterator begin() const { return data_; }
		ConstIterator end() const { return data_ + size_; }
	private:
		const T* data_;
		uint32_t size_;
		bool owns_data_;
	};
	
	
	class Arguments {
	public:
		typedef ArrayOrRef<const Value> Container;
		typedef typename Container::ValueType ValueType;
		typedef typename Container::Iterator Iterator;
		typedef typename Container::ConstIterator ConstIterator;
		// STL compatibility:
		typedef ValueType value_type;
		typedef Iterator iterator;
		typedef ConstIterator const_iterator;
		
		Arguments() {}
		Arguments(const Value* data, size_t sz) : data_(data, sz) {}
		Arguments(const Value* data, size_t sz, const Symbol* names, size_t num_names) : data_(data, sz), names_(names, num_names) {}
		Arguments(const Arguments& other) = default;
		Arguments(Arguments&& other) = default;
		Arguments& operator=(const Arguments& other) = default;
		Arguments& operator=(Arguments&& other) = default;
		
		Iterator begin() { return data_.begin(); }
		Iterator end() { return data_.end(); }
		ConstIterator begin() const { return data_.begin(); }
		ConstIterator end() const { return data_.end(); }
		
		const Value& operator[](size_t idx) const {
			return data_[idx];
		}
		
		size_t size() const { return data_.size(); }
		
		const ArrayOrRef<const Symbol>& names() const { return names_; }
		
		void take_ownership() {
			data_.take_ownership();
			names_.take_ownership();
		}
		
		void clear() {
			data_.clear();
			names_.clear();
		}
	private:
		ArrayOrRef<const Value> data_;
		ArrayOrRef<const Symbol> names_;
	};
}

#endif /* end of include guard: ARGUMENTS_H_5M3JAXPP */
