#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/gc.hpp"
#include "snow/value.hpp"
#include "snow/symbol.hpp"
#include "snow/type.hpp"

struct SnArguments;

typedef struct SnObject {
	struct {
		union {
			struct {
				unsigned flags       : 16;
				unsigned page_offset : 16;
			};
		};
	} gc_info;
	uint32_t num_alloc_members;
	snow::VALUE* members;
	const snow::Type* type;
	struct SnObject* cls;
} SnObject;

namespace snow {
	SnObject* get_object_class();
	SnObject* create_object(SnObject* cls, size_t num_constructor_args, VALUE* args);
	SnObject* create_object_with_arguments(SnObject* cls, const struct SnArguments* constructor_args);
	SnObject* create_object_without_initialize(SnObject* cls);

	// Instance variable related
	VALUE object_get_instance_variable(VALUE obj, Symbol name);
	VALUE object_set_instance_variable(VALUE obj, Symbol name, VALUE val);
	VALUE object_get_instance_variable_by_index(const SnObject* obj, int32_t idx);
	VALUE object_set_instance_variable_by_index(SnObject* obj, int32_t idx, VALUE val);
	int32_t object_get_index_of_instance_variable(const SnObject* obj, Symbol name); // -1 if not found
	int32_t object_get_or_create_index_of_instance_variable(const SnObject* object, Symbol name);

	// Meta-class related
	bool object_give_meta_class(SnObject* obj);
	VALUE _object_define_method(SnObject* obj, Symbol name, VALUE func);
	#define object_define_method(OBJ, NAME, FUNC) _object_define_method(OBJ, snow::sym(NAME), snow::create_function(FUNC, snow::sym(#FUNC)))
	VALUE _object_define_property(SnObject* obj, Symbol name, VALUE getter, VALUE setter);
	#define object_define_property(OBJ, NAME, GETTER, SETTER) _object_define_property(OBJ, snow::sym(NAME), snow::create_function(GETTER, snow::sym(#GETTER)), snow::create_function(SETTER, snow::sym(#SETTER)))
	VALUE object_set_property_or_define_method(SnObject* self, Symbol name, VALUE val);

	// Object type related
	INLINE ValueType type_of(const void* val) {
		if (!val) return NilType;
		const uintptr_t t = (uintptr_t)val & ValueTypeMask;
		if (t & 0x1) return IntegerType;
		return (ValueType)t;
	}

	INLINE SnObject* object_get_class(const SnObject* obj) {
		return obj->cls;
	}

	INLINE const Type* get_object_type(const SnObject* obj) {
		return obj->type;
	}

	INLINE bool object_is_of_type(const SnObject* obj, const Type* check_type) {
		return get_object_type(obj) == check_type;
	}

	INLINE void* object_get_private(const SnObject* obj, const Type* check_type) {
		ASSERT(check_type);
		if (object_is_of_type(obj, check_type)) {
			if (UNLIKELY(check_type->data_size + sizeof(SnObject) > SN_CACHE_LINE_SIZE)) {
				// if the object size is big, the private data is allocated separately,
				// with a pointer to it immediately following the object.
				return *(void**)(obj + 1);
			}
			// if the object size is small (most often the case), the private data is
			// allocated immediately following the object.
			return (void*)(obj + 1);
		}
		return NULL;
	}
	
	
	template <typename T>
	T* object_get_private(SnObject* obj, const Type* check_type) {
		return reinterpret_cast<T*>(object_get_private(obj, check_type));
	}
	
	template <typename T>
	const T* object_get_private(const SnObject* obj, const Type* check_type) {
		return reinterpret_cast<const T*>(object_get_private(obj, check_type));
	}
	
	template <typename T>
	T* value_get_private(VALUE val, const Type* check_type) {
		if (is_object(val)) {
			return object_get_private<T>((SnObject*)val, check_type);
		}
		return NULL;
	}
	
	INLINE bool value_is_of_type(VALUE val, const Type* check_type) {
		if (is_object(val)) {
			return object_is_of_type((SnObject*)val, check_type);
		}
		return false;
	}
	
	INLINE bool value_is_of_type(VALUE val, ValueType check_type) {
		return type_of(val) == check_type;
	}
}

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
