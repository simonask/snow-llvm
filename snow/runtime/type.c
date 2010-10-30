#include "snow/type.h"
#include "snow/object.h"

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
CAPI SnObject* snow_create_function_call_context_prototype();
CAPI SnObject* snow_create_pointer_prototype();

static SnObject* prototypes[SnNumTypes];

SnObject** snow_get_prototypes() { return prototypes; }

SnObject* snow_get_prototype_for_type(SnType type) {
	if (prototypes[type] == NULL) {
		switch (type) {
			case SnIntegerType:  prototypes[type] = snow_create_integer_prototype(); break;
			case SnNilType:      prototypes[type] = snow_create_nil_prototype(); break;
			case SnFalseType: {
				if (prototypes[SnTrueType]) prototypes[type] = prototypes[SnTrueType];
				else prototypes[type] = snow_create_boolean_prototype();
				break;
			}
			case SnTrueType: {
				if (prototypes[SnFalseType]) prototypes[type] = prototypes[SnFalseType];
				else prototypes[type] = snow_create_boolean_prototype();
				break;
			}
			case SnSymbolType:   prototypes[type] = snow_create_symbol_prototype(); break;
			case SnFloatType:    prototypes[type] = snow_create_float_prototype(); break;
			case SnObjectType:   prototypes[type] = snow_create_object_prototype(); break;
			case SnStringType:   prototypes[type] = snow_create_string_prototype(); break;
			case SnArrayType:    prototypes[type] = snow_create_array_prototype(); break;
			case SnMapType:      prototypes[type] = snow_create_map_prototype(); break;
			case SnFunctionType: prototypes[type] = snow_create_function_prototype(); break;
			case SnPointerType:  prototypes[type] = snow_create_pointer_prototype(); break;
			default: {
				ASSERT(false && "Requested prototype for invalid type.");
				break;
			}
		}
	}
	return prototypes[type];
}


SnType snow_type_of(VALUE val) {
	if (!val) return SnNilType;
	const uintptr_t t = (uintptr_t)val & SnTypeMask;
	if (t == 0x0) return ((SnObjectBase*)val)->type;
	if (t & 0x1) return SnIntegerType;
	return (SnType)t;
}

bool snow_is_object(VALUE val) {
	return val && (((uintptr_t)val & SnTypeMask) == SnAnyType);
}

