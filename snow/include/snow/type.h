#pragma once
#ifndef TYPE_H_ZR7ZWP35
#define TYPE_H_ZR7ZWP35

#include "snow/basic.h"
#include "snow/value.h"

INLINE bool snow_is_immediate_type(SnType type) {
	return type < SnTypeMask && type != SnAnyType;
}

CAPI size_t snow_size_of_type(SnType type);

#endif /* end of include guard: TYPE_H_ZR7ZWP35 */
