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
		AnyObjectPtr() : object_(NULL) {}
		AnyObjectPtr(Object* object) : object_(object) {}
		AnyObjectPtr(const Value& val) { assign(val); }
		AnyObjectPtr(const AnyObjectPtr& other) : object_(other.object_) {}
		AnyObjectPtr& operator=(const Value& val) { assign(val); return *this; }
		AnyObjectPtr& operator=(const AnyObjectPtr& other) { object_ = other.object_; return *this; }
		AnyObjectPtr& operator=(Object* obj) { assign(obj); return *this; }
		
		Object* value() const { return (Object*)object_.value(); }
		const Type* type() const;
		operator VALUE() const { return object_; }
		operator const Value&() const { return object_; }
		operator Object*() const { return value(); }
		Object* operator->() const { return value(); }
		Object& operator*() const { return *value(); }
		bool operator==(const Value& other) const { return object_ == other; }
		bool operator!=(const Value& other) const { return object_ != other; }
		bool operator==(Object* obj) const { return object_ == obj; }
		bool operator!=(Object* obj) const { return object_ != obj; }
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
	
	template <typename T>
	class ObjectPtr {
	public:
		typedef typename RemoveConst<T>::Result NonConstT;
		typedef const NonConstT ConstT;
		
		struct Null;
		
		ObjectPtr() : object_(NULL), priv_(NULL) {}
		ObjectPtr(const Value& val) : object_(val) { assign(object_); }
		ObjectPtr(const AnyObjectPtr& val) { assign(val); }
		template <typename U> ObjectPtr(const ObjectPtr<U>& other) = delete;
		ObjectPtr(const ObjectPtr<NonConstT>& other) : object_(other.object_), priv_(other.priv_) {}
		ObjectPtr(const ObjectPtr<ConstT>& other) : object_(other.object_), priv_(other.priv_) {}
		ObjectPtr(Null*) : object_(), priv_(NULL) {}
		
		template <typename U> void operator=(const ObjectPtr<U>& other) = delete;
		ObjectPtr<T>& operator=(const ObjectPtr<NonConstT>& other) { object_ = other.object_; priv_ = other.priv_; return *this; }
		ObjectPtr<T>& operator=(const ObjectPtr<ConstT>& other) { object_ = other.object_; priv_ = other.priv_; return *this; }
		ObjectPtr<T>& operator=(const AnyObjectPtr& other) { assign(other); return *this; }
		ObjectPtr<T>& operator=(const Value& other) { assign(AnyObjectPtr(other)); return *this; }
		ObjectPtr<T>& operator=(Null*) { assign(NULL); return *this; }
		
		Object* value() const { return object_.value(); }
		operator VALUE() const { return object_; }
		operator const AnyObjectPtr&() const { return object_; }
		operator const Value&() const { return object_; }
		T* operator->() const { return priv_; }
		T& operator*() const { return *priv_; }
		bool operator==(const Value& other) const { return object_ == other; }
		bool operator!=(const Value& other) const { return object_ != other; }
		bool operator==(Null*) const { return object_ == NULL; }
		bool operator!=(Null*) const { return object_ != NULL; }
	private:
		template <typename U> friend class ObjectPtr;
		ObjectPtr(const AnyObjectPtr& o, T* p) : object_(o), priv_(p) {}
		void assign(const AnyObjectPtr& val);
		
		AnyObjectPtr object_;
		T* priv_;
	};
}

#include "snow/objectdata.hpp"

namespace snow {
	template <typename T>
	void ObjectPtr<T>::assign(const AnyObjectPtr& val) {
		if (val != NULL) {
			priv_ = object_get_private<T>(val);
			object_ = priv_ != NULL ? val : AnyObjectPtr();
		} else {
			priv_ = NULL;
			object_ = NULL;
		}
	}
}

#endif /* end of include guard: OBJECTPTR_HPP_DWKBYC9Z */
