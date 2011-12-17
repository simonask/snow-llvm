#pragma once
#ifndef CLASS_H_JLNQIJLJ
#define CLASS_H_JLNQIJLJ

#include "snow/object.h"

CAPI struct SnInternalType SnClassType;

enum SnMethodType {
	SnMethodTypeNone,
	SnMethodTypeFunction,
	SnMethodTypeProperty,
};

struct SnProperty {
	VALUE getter;
	VALUE setter;
};

struct SnMethod {
	enum SnMethodType type;
	SnSymbol name;
	union {
		VALUE function;
		struct SnProperty* property;
	};
};

CAPI SnObject* snow_get_class_class();
CAPI bool snow_is_class(VALUE val);

CAPI SnObject* snow_create_class(SnSymbol name, SnObject* super);
CAPI SnObject* snow_create_class_for_type(SnSymbol name, const SnInternalType* type);
CAPI SnObject* snow_create_meta_class(SnObject* super);
CAPI bool snow_class_is_meta(const SnObject* cls);
CAPI const char* snow_class_get_name(const SnObject* cls);

// Instance variables
CAPI int32_t snow_class_get_index_of_instance_variable(const SnObject* cls, SnSymbol name);
CAPI int32_t snow_class_define_instance_variable(SnObject* cls, SnSymbol name);
CAPI size_t snow_class_get_num_instance_variables(const SnObject* cls);

// Methods and properties
CAPI bool snow_class_lookup_method(const SnObject* cls, SnSymbol name, struct SnMethod* out_method);
CAPI void snow_class_get_method(const SnObject* cls, SnSymbol name, struct SnMethod* out_method); // throws if not found!
CAPI SnObject* _snow_class_define_method(SnObject* cls, SnSymbol name, VALUE function);
#define snow_class_define_method(CLS, NAME, FUNC) _snow_class_define_method(CLS, snow_sym(NAME), snow_create_function(FUNC, snow_sym(#FUNC)))
CAPI SnObject* _snow_class_define_property(SnObject* cls, SnSymbol name, VALUE getter, VALUE setter);
#define snow_class_define_property(CLS, NAME, GETTER, SETTER) _snow_class_define_property(CLS, snow_sym(NAME), snow_create_function(GETTER, snow_sym(#GETTER)), snow_create_function(SETTER, snow_sym(#SETTER)))
CAPI VALUE snow_class_get_initialize(const SnObject* cls);

#endif /* end of include guard: CLASS_H_JLNQIJLJ */
