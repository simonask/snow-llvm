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
		SnType type;
		void* _; // for alignment
	};
} SnObjectBase;

typedef struct SnObject {
	SnObjectBase base;
	struct SnObject* prototype;
	struct SnMap* members;
	struct SnMap* properties;
	struct SnArray* included_modules;
} SnObject;

CAPI SnObject* snow_create_object(SnObject* prototype);
CAPI VALUE snow_object_get_member(SnObject* object, VALUE self_for_properties, SnSymbol member);
CAPI VALUE snow_object_set_member(SnObject* object, VALUE self_for_properties, SnSymbol member, VALUE val);
CAPI void snow_object_set_property_getter(SnObject* object, SnSymbol member, VALUE getter);
CAPI void snow_object_set_property_setter(SnObject* object, SnSymbol member, VALUE setter);

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
