#pragma once
#ifndef OBJECTDATA_HPP_PDSPILNN
#define OBJECTDATA_HPP_PDSPILNN

#include "snow/objectptr.hpp"
#include "snow/util.hpp"
#include "snow/value.hpp"

namespace snow {
	struct Class;
	
	struct Object {
		uint32_t refcount;
		uint32_t num_alloc_members;
		Value* members;
		const Type* type;
		ObjectPtr<Class> cls;
		Object() : refcount(0), num_alloc_members(0), members(NULL), type(NULL) {}
		~Object() { dealloc_range(members, num_alloc_members); }
	};
	
	INLINE bool object_is_of_type(const AnyObjectPtr& object, const Type* check_type) {
		return object->type == check_type;
	}
	
	INLINE void* object_get_private(const AnyObjectPtr& obj, const Type* check_type) {
		ASSERT(check_type);
		if (object_is_of_type(obj, check_type)) {
			if (UNLIKELY(check_type->data_size + sizeof(Object) > SN_CACHE_LINE_SIZE)) {
				// if the object size is big, the private data is allocated separately,
				// with a pointer to it immediately following the object.
				return *(void**)(obj.object() + 1);
			}
			// if the object size is small (most often the case), the private data is
			// allocated immediately following the object.
			return (void*)(obj.object() + 1);
		}
		return NULL;
	}
	
	INLINE bool value_is_of_type(const Value& val, const Type* check_type) {
		if (val.is_object()) {
			return object_is_of_type(val, check_type);
		}
		return false;
	}
	
	template <typename T>
	T* object_get_private(const AnyObjectPtr& obj) {
		return reinterpret_cast<T*>(object_get_private(obj, get_type<T>()));
	}
	
	template <typename T>
	T* value_get_private(const Value& val) {
		if (val.is_object()) {
			return object_get_private<T>((Object*)val.value(), get_type<T>());
		}
		return NULL;
	}
}

#endif /* end of include guard: OBJECTDATA_HPP_PDSPILNN */
