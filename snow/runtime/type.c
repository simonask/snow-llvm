#include "snow/type.h"
#include "snow/object.h"
#include "snow/exception.h"
#include "snow/gc.h"

#include <stdlib.h>

// Declarations
CAPI SnObject* snow_create_integer_prototype();
CAPI SnObject* snow_create_float_prototype();
CAPI SnObject* snow_create_nil_prototype();
CAPI SnObject* snow_create_boolean_prototype();
CAPI SnObject* snow_create_symbol_prototype();
CAPI SnObject* snow_create_object_prototype();
CAPI SnObject* snow_create_string_prototype();
CAPI SnObject* snow_create_array_prototype();
CAPI SnObject* snow_create_map_prototype();
CAPI SnObject* snow_create_function_prototype();
CAPI SnObject* snow_create_call_frame_prototype();
CAPI SnObject* snow_create_arguments_prototype();
CAPI SnObject* snow_create_pointer_prototype();
CAPI SnObject* snow_create_fiber_prototype();

static VALUE** get_prototype_roots() {
	static VALUE** roots = NULL;
	if (!roots) {
		roots = (VALUE**)malloc(sizeof(VALUE*) * SnNumTypes);
		memset(roots, 0, sizeof(VALUE*) * SnNumTypes);
	}
	return roots;
}

SnObject* snow_get_prototype_for_type(SnType type) {
	ASSERT(type < SnNumTypes);
	
	VALUE** roots = get_prototype_roots();
	if (roots[type] == NULL) {
		SnObject* prototype = NULL;
		
		switch (type) {
			case SnIntegerType:  prototype = snow_create_integer_prototype(); break;
			case SnNilType:      prototype = snow_create_nil_prototype(); break;
			case SnFalseType: {
				if (roots[SnTrueType]) prototype = (SnObject*)*roots[SnTrueType];
				else prototype = snow_create_boolean_prototype();
				break;
			}
			case SnTrueType: {
				if (roots[SnFalseType]) prototype = (SnObject*)*roots[SnFalseType];
				else prototype = snow_create_boolean_prototype();
				break;
			}
			case SnSymbolType:   prototype = snow_create_symbol_prototype(); break;
			case SnFloatType:    prototype = snow_create_float_prototype(); break;
			case SnObjectType:   prototype = snow_create_object_prototype(); break;
			case SnStringType:   prototype = snow_create_string_prototype(); break;
			case SnArrayType:    prototype = snow_create_array_prototype(); break;
			case SnMapType:      prototype = snow_create_map_prototype(); break;
			case SnFunctionType: prototype = snow_create_function_prototype(); break;
			case SnCallFrameType: prototype = snow_create_call_frame_prototype(); break;
			case SnArgumentsType: prototype = snow_create_arguments_prototype(); break;
			case SnPointerType:  prototype = snow_create_pointer_prototype(); break;
			case SnFiberType:    prototype = snow_create_fiber_prototype(); break;
			default: {
				snow_throw_exception_with_description("Invalid object type encountered (memory corruption is likely): %d\n", type);
				break;
			}
		}
		
		ASSERT(prototype);
		VALUE* root = snow_gc_create_root(prototype);
		roots[type] = root;
	}
	return (SnObject*)*roots[type];
}
