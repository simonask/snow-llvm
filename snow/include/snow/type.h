#pragma once
#ifndef TYPE_H_ZR7ZWP35
#define TYPE_H_ZR7ZWP35

#include "snow/basic.h"
#include "snow/value.h"

INLINE bool snow_is_immediate_type(SnValueType type) {
	return type < SnValueTypeMask && type != SnPointerType;
}

#endif /* end of include guard: TYPE_H_ZR7ZWP35 */
