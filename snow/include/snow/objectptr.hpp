#pragma once
#ifndef OBJECTPTR_HPP_DWKBYC9Z
#define OBJECTPTR_HPP_DWKBYC9Z

#include "snow/value.h"
#include "snow/util.hpp"
#include "snow/type.hpp"

namespace snow {
	template <typename T>
	class ObjectPtr {
	public:
		typedef typename ReplicateConstPtr<T, SnObject>::Result Ptr;
		
		ObjectPtr() : object_(NULL), priv_(NULL) {}
		ObjectPtr(VALUE val) { assign(val); }
		ObjectPtr(Ptr obj) { assign(obj); }
		ObjectPtr(const ObjectPtr<T>& other) : object_(other.object_), priv_(other.priv_) {}
		ObjectPtr<T>& operator=(const ObjectPtr<T>& other) { object_ = other.object_; priv_ = other.priv_; return *this; }
		ObjectPtr<T>& operator=(VALUE val) { assign(val); return *this; }
		ObjectPtr<T>& operator=(Ptr obj) { assign(obj); return *this; }
		
		operator Ptr() const { return object_; }
		T* operator->() const { return priv_; }
		T& operator*() { return *priv_; }
		const T& operator*() const { return *priv_; }
		bool operator==(VALUE other) const { return object_ == other; }
		bool operator!=(VALUE other) const { return object_ != other; }
		bool operator==(const ObjectPtr<T>& other) const { return object_ == other.object_; }
		bool operator!=(const ObjectPtr<T>& other) const { return object_ != other.object_; }
	private:
		void assign(VALUE val) {
			if (snow_is_object(val)) {
				priv_ = object_get_private<T>((Ptr)val, snow::get_type<T>());
				object_ = priv_ ? (Ptr)val : NULL;
			} else {
				priv_ = NULL;
				object_ = NULL;
			}
		}
		
		void assign(Ptr obj) {
			if (obj != NULL) {
				priv_ = object_get_private<T>(obj, snow::get_type<T>());
				object_ = priv_ ? obj : NULL;
			} else {
				object_ = NULL;
				priv_ = NULL;
			}
		}
		
		Ptr object_;
		T* priv_;
	};
}

#endif /* end of include guard: OBJECTPTR_HPP_DWKBYC9Z */
