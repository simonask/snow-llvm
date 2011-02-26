#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/value.h"
#include "snow/symbol.h"

struct SnImmediateMap;
struct SnMap;
struct SnArray;

typedef struct SnObjectBase {
	union {
		struct {
			unsigned gc_flags       : 8;
			unsigned gc_page_offset : 8;
			SnType type             : 8;
			unsigned frozen         : 1;
		};
		void* _; // for alignment
	};
} SnObjectBase;

typedef struct SnProperty {
	SnSymbol name;
	VALUE getter;
	VALUE setter;
} SnProperty;

typedef struct SnObject {
	SnObjectBase base;
	struct SnObject* prototype;
	struct SnMap* members;
	SnProperty* properties;
	size_t num_properties;
	struct SnArray* included_modules;
	SnSymbol name;
} SnObject;

CAPI SnObject* snow_create_object(SnObject* prototype);
CAPI void snow_finalize_object(SnObject* obj);
CAPI VALUE snow_object_get_member(SnObject* object, VALUE self_for_properties, SnSymbol member);
CAPI VALUE snow_object_set_member(SnObject* object, VALUE self_for_properties, SnSymbol member, VALUE val);
CAPI void snow_object_set_property_getter(SnObject* object, SnSymbol member, VALUE getter);
CAPI void snow_object_set_property_setter(SnObject* object, SnSymbol member, VALUE setter);
CAPI void snow_object_define_property(SnObject* object, SnSymbol name, VALUE getter, VALUE setter);
CAPI bool snow_include_module(SnObject* object, SnObject* module);

#define SN_DEFINE_PROPERTY(OBJECT, NAME, GETTER, SETTER) snow_object_define_property(OBJECT, snow_sym(NAME), snow_create_method(GETTER, snow_sym(#GETTER), 0), snow_create_method(SETTER, snow_sym(#GETTER), 1))

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
