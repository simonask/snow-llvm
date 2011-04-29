#pragma once
#ifndef CLASS_H_JLNQIJLJ
#define CLASS_H_JLNQIJLJ

#include "snow/basic.h"
#include "snow/object.h"

typedef enum SnFieldFlags {
	SnFieldNoFlags = 0x0,    // public
	SnFieldRawPointer = 0x1, // read-only by Snow code
} SnFieldFlags;

typedef struct SnField {
	SnSymbol name;
	uint8_t flags;
} SnField;

typedef struct SnProperty {
	VALUE getter;
	VALUE setter;
} SnProperty;

typedef enum SnMethodType {
	SnMethodTypeFunction,
	SnMethodTypeProperty,
	SnMethodTypeReference, // inherited method
} SnMethodType;

typedef struct SnMethod {
	SnSymbol name;
	SnMethodType type;
	union {
		VALUE function;
		SnProperty property;
		const struct SnMethod* reference;
	};
} SnMethod;

typedef struct SnClass {
	SnObject base;
	SnType internal_type;
	SnSymbol name;
	struct SnClass* super;
	SnField* fields;
	SnMethod** methods;
	SnObject** extensions;
	uint32_t num_fields;
	uint32_t num_methods;
	uint32_t num_extensions;
} SnClass;

CAPI SnClass* snow_define_class(SnSymbol name, SnClass* super, size_t num_fields, const SnField* fields, size_t num_methods, const SnMethod* methods);
CAPI SnClass* snow_get_class_class();

CAPI size_t snow_class_define_field(SnClass* cls, SnSymbol name, uint8_t flags); // return: index
CAPI ssize_t snow_class_index_of_field(const SnClass* cls, SnSymbol name, uint8_t flags);
INLINE size_t snow_class_get_num_fields(const SnClass* cls) { return cls->num_fields; }

CAPI void snow_class_define_method(SnClass* cls, SnSymbol name, VALUE function);
CAPI void snow_class_define_property(SnClass* cls, SnSymbol name, VALUE getter, VALUE setter);
CAPI size_t snow_class_extend_with_module(SnClass* cls, SnObject* module);

CAPI SnClass* snow_meta_class_define_method(SnClass* cls, SnSymbol name, VALUE functor);
CAPI SnClass* snow_meta_class_define_property(SnClass* cls, SnSymbol name, VALUE getter, VALUE setter);

CAPI const SnMethod* snow_find_method(SnClass* cls, SnSymbol name);
CAPI const SnMethod* snow_resolve_method_reference(const SnMethod* method);
CAPI const SnMethod* snow_find_and_resolve_method(SnClass* cls, SnSymbol name);

#define SN_METHOD(NAME, FUNCTION, NUM_ARGS) { snow_sym(NAME), SnMethodTypeFunction, .function = snow_create_method(FUNCTION, snow_sym(NAME), NUM_ARGS) }
#define SN_PROPERTY(NAME, GETTER, SETTER) { \
	.name = snow_sym(NAME), \
	.type = SnMethodTypeProperty, \
	.property = (struct SnProperty){ \
		.getter = (GETTER ? snow_create_method(GETTER, snow_sym(NAME), 0) : NULL), \
		.setter = (SETTER ? snow_create_method(SETTER, snow_sym(NAME), 1) : NULL) \
	} \
}

#endif /* end of include guard: CLASS_H_JLNQIJLJ */
