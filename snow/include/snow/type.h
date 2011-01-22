#pragma once
#ifndef TYPE_H_ZR7ZWP35
#define TYPE_H_ZR7ZWP35

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/object.h"

INLINE SnType snow_type_of(VALUE val) {
	if (!val) return SnNilType;
	const uintptr_t t = (uintptr_t)val & SnTypeMask;
	if (t == 0x0) return ((SnObjectBase*)val)->type;
	if (t & 0x1) return SnIntegerType;
	return (SnType)t;
}

CAPI struct SnObject** snow_get_prototypes();
CAPI struct SnObject* snow_get_prototype_for_type(SnType type);


#endif /* end of include guard: TYPE_H_ZR7ZWP35 */
