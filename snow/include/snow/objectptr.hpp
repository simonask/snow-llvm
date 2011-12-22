#pragma once
#ifndef OBJECTPTR_HPP_DWKBYC9Z
#define OBJECTPTR_HPP_DWKBYC9Z

#include "snow/value.hpp"
#include "snow/util.hpp"
#include "snow/type.hpp"

namespace snow {
	struct Object;
	class AnyObjectPtr;
	
	template <typename T>
	class ObjectPtr {
	public:
		typedef typename RemoveConst<T>::Result NonConstT;
		typedef const NonConstT ConstT;
		
		struct Null;
		
		ObjectPtr() : object_(NULL), priv_(NULL) {}
		ObjectPtr(const Value& val) { assign(val); }
		ObjectPtr(const ObjectPtr<T>& other) : object_(other.object_), priv_(other.priv_) {}
		ObjectPtr(Null*) : object_(), priv_(NULL) {}
		template <typename U> ObjectPtr(const ObjectPtr<U>& other) = delete;
		ObjectPtr<T>& operator=(const ObjectPtr<NonConstT>& other) { object_ = other.object_; priv_ = other.priv_; return *this; }
		ObjectPtr<T>& operator=(const ObjectPtr<ConstT>& other) { object_ = other.object_; priv_ = other.priv_; return *this; }
		ObjectPtr<T>& operator=(const Value& val) { assign(val); return *this; }
		ObjectPtr<T>& operator=(const AnyObjectPtr& other);
		ObjectPtr<T>& operator=(Null*) { assign(NULL); return *this; }
		
		VALUE value() const { return object_.value(); }
		Object* object() const { return (Object*)object_.value(); }
		operator ObjectPtr<const T>() const { return ObjectPtr<const T>(object_, priv_); }
		operator const Value&() const { return object_; }
		T* operator->() const { return priv_; }
		T& operator*() const { return *priv_; }
		bool operator==(const Value& other) const { return object_ == other; }
		bool operator!=(const Value& other) const { return object_ != other; }
		bool operator==(Null*) const { return object_ == NULL; }
		bool operator!=(Null*) const { return object_ != NULL; }
		bool operator==(const ObjectPtr<T>& other) const { return object_ == other.object_; }
		bool operator!=(const ObjectPtr<T>& other) const { return object_ != other.object_; }
	private:
		template <typename U> friend class ObjectPtr;
		ObjectPtr(const Value& o, T* p) : object_(o), priv_(p) {}
		void assign(const Value& val);
		
		Value object_;
		T* priv_;
	};
	
	class AnyObjectPtr {
	public:
		struct Null;
		
		AnyObjectPtr() : object_(NULL) {}
		AnyObjectPtr(Object* object) : object_(object) {}
		AnyObjectPtr(const Value& val) { assign(val); }
		AnyObjectPtr(const AnyObjectPtr& other) : object_(other.object_) {}
		template <typename T>
		AnyObjectPtr(const ObjectPtr<T>& other) : object_(other) {}
		AnyObjectPtr& operator=(const AnyObjectPtr& other) { object_ = other.object_; return *this; }
		template <typename T>
		AnyObjectPtr& operator=(const ObjectPtr<T>& other) { object_ = other.value(); return *this; }
		AnyObjectPtr& operator=(Null*) { assign(NULL); return *this; }
		
		VALUE value() const { return object_.value(); }
		Object* object() const { return (Object*)object_.value(); }
		operator const Value() const { return object_; }
		operator const Value&() const { return object_; }
		template <typename T>
		operator ObjectPtr<T>() const { return ObjectPtr<T>(object_); }
		Object* operator->() const { return object(); }
		Object& operator*() const { return *object(); }
		bool operator==(const Value& other) const { return object_ == other; }
		bool operator!=(const Value& other) const { return object_ != other; }
	private:
		Value object_;
		
		void assign(const Value& val) {
			if (val.is_object()) {
				object_ = val;
			} else {
				object_ = NULL;
			}
		}
	};
}

#include "snow/objectdata.hpp"

namespace snow {
	template <typename T>
	void ObjectPtr<T>::assign(const Value& val) {
		if (val.is_object()) {
			priv_ = object_get_private<T>(val);
			object_ = priv_ != NULL ? val : Value();
		} else {
			priv_ = NULL;
			object_ = NULL;
		}
	}
	
	template <typename T>
	ObjectPtr<T>& ObjectPtr<T>::operator=(const AnyObjectPtr& other) {
		assign(other);
		return *this;
	}
}

#endif /* end of include guard: OBJECTPTR_HPP_DWKBYC9Z */
