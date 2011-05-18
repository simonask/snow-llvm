#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/gc.h"
#include "snow/value.h"
#include "snow/symbol.h"

struct SnClass;

typedef struct SnObject {
	SnGCInfo gc_info;
	SnType type;
	struct SnClass* cls;
	VALUE* members;
	uint32_t num_members;
} SnObject;

CAPI struct SnClass* snow_get_object_class();
CAPI SnObject* snow_create_object(struct SnClass* cls);

CAPI VALUE snow_object_get_field(VALUE obj, SnSymbol name);
CAPI VALUE snow_object_set_field(VALUE obj, SnSymbol name, VALUE val);
CAPI VALUE snow_object_get_field_by_index(const SnObject* obj, int32_t idx);
CAPI VALUE snow_object_set_field_by_index(SnObject* obj, int32_t idx, VALUE val);
CAPI void snow_object_give_meta_class(SnObject* obj);
CAPI VALUE _snow_object_define_method(VALUE obj, SnSymbol name, VALUE func);
#define snow_object_define_method(OBJ, NAME, FUNC, NUM_ARGS) _snow_object_define_method(OBJ, snow_sym(NAME), snow_create_method(FUNC, snow_sym(#FUNC), NUM_ARGS))
CAPI VALUE snow_object_set_property_or_define_method(VALUE self, SnSymbol name, VALUE val);

INLINE SnType snow_type_of(const void* val) {
	if (!val) return SnNilType;
	const uintptr_t t = (uintptr_t)val & SnTypeMask;
	if (t == 0x0) return ((SnObject*)val)->type;
	if (t & 0x1) return SnIntegerType;
	return (SnType)t;
}

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
