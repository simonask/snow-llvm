#pragma once
#ifndef OBJECTPTR_HPP_DWKBYC9Z
#define OBJECTPTR_HPP_DWKBYC9Z

#include "snow/value.hpp"
#include "snow/util.hpp"
#include "snow/type.hpp"

namespace snow {
	struct Object;
	class AnyObjectPtr;
	
	class AnyObjectPtr {
	public:
		AnyObjectPtr() : object_(nullptr) {}
		AnyObjectPtr(Object* object) : object_(object) {}
		//AnyObjectPtr(VALUE val) { assign(val); }
		AnyObjectPtr(Value val) { assign(val); }
		AnyObjectPtr(VALUE val) { assign(val); }
		AnyObjectPtr(const AnyObjectPtr& other) : object_(other.object_) {}
		AnyObjectPtr& operator=(Value val) { assign(val); return *this; }
		AnyObjectPtr& operator=(AnyObjectPtr other) { object_ = other.object_; return *this; }
		AnyObjectPtr& operator=(Object* obj) { assign(obj); return *this; }
		
		Object* value() const { return (Object*)object_.value(); }
		const Type* type() const;
		operator VALUE() const { return object_; }
		operator Value() const { return object_; }
		operator Object*() const { return value(); }
		Object* operator->() const { return value(); }
		Object& operator*() const { return *value(); }
		bool operator==(Value other) const { return object_ == other; }
		bool operator!=(Value other) const { return object_ != other; }
		bool operator==(Object* obj) const { return object_ == obj; }
		bool operator!=(Object* obj) const { return object_ != obj; }
	private:
		Value object_;
		
		void assign(Value val) {
			if (val.is_object()) {
				object_ = val;
			} else {
				object_ = nullptr;
			}
		}
	};
	
	template <typename T>
	class ObjectPtr {
	public:
		typedef typename RemoveConst<T>::Result NonConstT;
		typedef const NonConstT ConstT;
		
		struct Null;
		
		ObjectPtr() {}
		ObjectPtr(Value val) : object_(val) { assign(object_); }
		ObjectPtr(VALUE val) : object_(val) { assign(object_); }
		ObjectPtr(const AnyObjectPtr& val) { assign(val); }
		template <typename U> ObjectPtr(const ObjectPtr<U>& other) = delete;
		ObjectPtr(const ObjectPtr<NonConstT>& other) : object_(other.object_) {}
		ObjectPtr(const ObjectPtr<ConstT>& other) : object_(other.object_) {}
		
		template <typename U> void operator=(ObjectPtr<U> other) = delete;
		ObjectPtr<T>& operator=(ObjectPtr<NonConstT> other) { object_ = other.object_; return *this; }
		ObjectPtr<T>& operator=(ObjectPtr<ConstT> other) { object_ = other.object_; return *this; }
		ObjectPtr<T>& operator=(AnyObjectPtr other) { assign(other); return *this; }
		ObjectPtr<T>& operator=(Value other) { assign(AnyObjectPtr(other)); return *this; }
		ObjectPtr<T>& operator=(VALUE other) { assign(AnyObjectPtr(other)); return *this; }
		
		Object* value() const { return object_.value(); }
		operator VALUE() const { return object_; }
		operator Value() const { return object_; }
		operator Object*() const { return object_; }
		operator AnyObjectPtr() const { return object_; }
		T* operator->() const { return get(); }
		T& operator*() const { return *get(); }
		bool operator==(Value other) const { return object_ == other; }
		bool operator!=(Value other) const { return object_ != other; }
		bool operator==(Null*) const { return object_ == nullptr; }
		bool operator!=(Null*) const { return object_ != nullptr; }
		
		T* get() const;
	private:
		template <typename U> friend class ObjectPtr;
		void assign(AnyObjectPtr val);
		
		AnyObjectPtr object_;
	};
}

#include "snow/objectdata.hpp"

namespace snow {
	template <typename T>
	void ObjectPtr<T>::assign(AnyObjectPtr val) {
		if (val != nullptr) {
			object_ = object_get_private<T>(val) != nullptr ? val : AnyObjectPtr();
		} else {
			object_ = nullptr;
		}
	}
	
	template <typename T>
	T* ObjectPtr<T>::get() const {
		return object_ != nullptr ? object_get_private<T>(object_) : nullptr;
	}
}

#endif /* end of include guard: OBJECTPTR_HPP_DWKBYC9Z */
