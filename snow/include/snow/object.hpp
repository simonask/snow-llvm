#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/gc.hpp"
#include "snow/value.h"
#include "snow/symbol.h"
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
	VALUE* members;
	const snow::Type* type;
	struct SnObject* cls;
} SnObject;

CAPI SnObject* snow_get_object_class();
CAPI SnObject* snow_create_object(SnObject* cls, size_t num_constructor_args, VALUE* args);
CAPI SnObject* snow_create_object_with_arguments(SnObject* cls, const struct SnArguments* constructor_args);
CAPI SnObject* snow_create_object_without_initialize(SnObject* cls);

// Instance variable related
CAPI VALUE snow_object_get_instance_variable(VALUE obj, SnSymbol name);
CAPI VALUE snow_object_set_instance_variable(VALUE obj, SnSymbol name, VALUE val);
CAPI VALUE snow_object_get_instance_variable_by_index(const SnObject* obj, int32_t idx);
CAPI VALUE snow_object_set_instance_variable_by_index(SnObject* obj, int32_t idx, VALUE val);
CAPI int32_t snow_object_get_index_of_instance_variable(const SnObject* obj, SnSymbol name); // -1 if not found

// Meta-class related
CAPI bool snow_object_give_meta_class(SnObject* obj);
CAPI VALUE _snow_object_define_method(SnObject* obj, SnSymbol name, VALUE func);
#define snow_object_define_method(OBJ, NAME, FUNC) _snow_object_define_method(OBJ, snow_sym(NAME), snow_create_function(FUNC, snow_sym(#FUNC)))
CAPI VALUE _snow_object_define_property(SnObject* obj, SnSymbol name, VALUE getter, VALUE setter);
#define snow_object_define_property(OBJ, NAME, GETTER, SETTER) _snow_object_define_property(OBJ, snow_sym(NAME), snow_create_function(GETTER, snow_sym(#GETTER)), snow_create_function(SETTER, snow_sym(#SETTER)))
CAPI VALUE snow_object_set_property_or_define_method(SnObject* self, SnSymbol name, VALUE val);

// Object type related
INLINE SnValueType snow_type_of(const void* val) {
	if (!val) return SnNilType;
	const uintptr_t t = (uintptr_t)val & SnValueTypeMask;
	if (t & 0x1) return SnIntegerType;
	return (SnValueType)t;
}

INLINE SnObject* snow_object_get_class(const SnObject* obj) {
	return obj->cls;
}

INLINE const snow::Type* snow_get_object_type(const SnObject* obj) {
	return obj->type;
}

INLINE bool snow_object_is_of_type(const SnObject* obj, const snow::Type* check_type) {
	return snow_get_object_type(obj) == check_type;
}

INLINE void* snow_object_get_private(const SnObject* obj, const snow::Type* check_type) {
	ASSERT(check_type);
	if (snow_object_is_of_type(obj, check_type)) {
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

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
