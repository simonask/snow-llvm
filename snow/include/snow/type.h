#pragma once
#ifndef TYPE_H_ZR7ZWP35
#define TYPE_H_ZR7ZWP35

#include "snow/basic.h"
#include "snow/value.h"

struct SnObject;

CAPI struct SnObject** snow_get_prototypes();
CAPI struct SnObject* snow_get_prototype_for_type(SnType type);


#endif /* end of include guard: TYPE_H_ZR7ZWP35 */
