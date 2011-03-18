#pragma once
#ifndef BOOLEAN_H_L8LG419
#define BOOLEAN_H_L8LG419

#include "snow/basic.h"
#include "snow/value.h"

INLINE bool snow_value_to_boolean(VALUE val) { return snow_eval_truth(val); }
INLINE VALUE snow_boolean_to_value(bool b) { return b ? SN_TRUE : SN_FALSE; }

#endif /* end of include guard: BOOLEAN_H_L8LG419 */
