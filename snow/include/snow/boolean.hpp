#pragma once
#ifndef BOOLEAN_H_L8LG419
#define BOOLEAN_H_L8LG419

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	INLINE bool value_to_boolean(VALUE val) { return is_truthy(val); }
	INLINE Value boolean_to_value(bool b) { return b ? SN_TRUE : SN_FALSE; }

	struct Class;
	ObjectPtr<Class> get_boolean_class();
}

#endif /* end of include guard: BOOLEAN_H_L8LG419 */
