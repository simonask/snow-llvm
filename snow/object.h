#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/value.h"
#include "snow/symbol.h"

struct SnType;
struct SnProcess;

typedef struct SnObject {
	const struct SnType* type;
	SN_P owner;
	void* data;
} SnObject;

CAPI SnObject* snow_create_object(SN_P);
CAPI VALUE snow_object_get_member(SN_P, SnObject* object, VALUE self /* for properties */, SnSymbol member);
CAPI VALUE snow_object_set_member(SN_P, SnObject* object, VALUE self /* for properties */, SnSymbol member, VALUE val);

CAPI const struct SnType* snow_get_object_type();

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
