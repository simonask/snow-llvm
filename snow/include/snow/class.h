#pragma once
#ifndef CLASS_H_JLNQIJLJ
#define CLASS_H_JLNQIJLJ

#include "snow/basic.h"
#include "snow/object.h"
#include "snow/function.h"

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
	};
} SnMethod;

typedef struct SnClass {
	SnObject base;
	SnType internal_type;
	SnSymbol name;
	struct SnClass* super;
	SnField* fields;
	SnMethod* methods;
	uint32_t num_fields;
	uint32_t num_methods;
	bool is_meta;
} SnClass;

CAPI SnClass* snow_create_class(SnSymbol name, SnClass* super);
CAPI SnClass* snow_create_meta_class(SnClass* base);
CAPI bool _snow_class_define_method(SnClass* cls, SnSymbol name, VALUE func);
CAPI bool _snow_class_define_property(SnClass* cls, SnSymbol name, VALUE getter, VALUE setter);
CAPI size_t _snow_class_define_field(SnClass* cls, SnSymbol name, SnFieldFlags flags);

CAPI SnClass* snow_get_class_class();

INLINE size_t snow_class_get_num_fields(const SnClass* cls) { return cls->num_fields; }
CAPI VALUE snow_class_get_method(const SnClass* cls, SnSymbol name);
CAPI int32_t snow_class_get_index_of_field(const SnClass* cls, SnSymbol name);
CAPI int32_t snow_class_get_or_define_index_of_field(SnClass* cls, SnSymbol name);

CAPI struct SnFunction* snow_create_method(SnFunctionPtr ptr, SnSymbol name, int32_t num_args);

#define snow_class_define_method(CLS, NAME, PTR, NUM_ARGS) _snow_class_define_method(CLS, snow_sym(NAME), snow_create_method(PTR, snow_sym(#PTR), NUM_ARGS))
#define snow_class_define_property(CLS, NAME, GETTER, SETTER) _snow_class_define_property(CLS, snow_sym(NAME), snow_create_method(GETTER, snow_sym(#GETTER), 0), snow_create_method(SETTER, snow_sym(#SETTER), 1))

#endif /* end of include guard: CLASS_H_JLNQIJLJ */
