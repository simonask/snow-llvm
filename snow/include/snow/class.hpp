#pragma once
#ifndef CLASS_H_JLNQIJLJ
#define CLASS_H_JLNQIJLJ

#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Class;
	typedef ObjectPtr<Class> ClassPtr;
	typedef ObjectPtr<const Class> ClassConstPtr;
	
	enum MethodType {
		MethodTypeNone,
		MethodTypeFunction,
		MethodTypeProperty,
		MethodTypeMissing,
	};
	
	struct Property {
		Value getter;
		Value setter;
	};
	
	struct Method {
		MethodType type;
		Symbol name;
		Value function;
		Property* property;
	};
	
	struct MethodQueryResult {
		MethodType type;
		VALUE result;
	};
	
	ObjectPtr<Class> get_class_class();
	bool is_class(Value val);
	
	ObjectPtr<Class> create_class(Symbol name, ClassPtr super);
	ObjectPtr<Class> create_class_for_type(Symbol name, const Type* type);
	ObjectPtr<Class> create_meta_class(ClassPtr cls);
	bool class_is_meta(ClassConstPtr cls);
	const char* class_get_name(ClassConstPtr cls);
	
	// Instance variables API
	int32_t class_get_index_of_instance_variable(ClassConstPtr cls, Symbol name);
	int32_t class_define_instance_variable(ClassPtr cls, Symbol name);
	size_t class_get_num_instance_variables(ClassConstPtr cls);
	
	// Methods and Properties API
	bool class_lookup_method(ClassConstPtr cls, Symbol name, MethodQueryResult* out_method);
	bool class_lookup_property_setter(ClassConstPtr cls, Symbol name, MethodQueryResult* out_method);
	ClassPtr _class_define_method(ClassPtr cls, Symbol name, Value function);
	ClassPtr _class_define_property(ClassPtr cls, Symbol name, Value getter, Value setter);
	void _register_binding(ClassPtr cls, Symbol name, void* func);
	#define SN_DEFINE_METHOD(CLS, NAME, FUNCPTR) snow::_class_define_method(CLS, snow::sym(NAME), snow::create_function(FUNCPTR, snow::sym(#FUNCPTR))); snow::_register_binding(CLS, snow::sym(NAME), (void*)FUNCPTR)
	#define SN_DEFINE_PROPERTY(CLS, NAME, GETTER, SETTER) snow::_class_define_property(CLS, snow::sym(NAME), snow::create_function(GETTER, snow::sym(#GETTER)), snow::create_function(SETTER, snow::sym(#SETTER))); snow::_register_binding(CLS, snow::sym(NAME), (void*)GETTER); snow::_register_binding(CLS, snow::sym(NAME ":"), (void*)SETTER)
	Value class_get_initialize(ClassConstPtr cls);
	
	ObjectPtr<Class> get_symbol_class();
}

#endif /* end of include guard: CLASS_H_JLNQIJLJ */
