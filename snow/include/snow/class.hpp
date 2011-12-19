#pragma once
#ifndef CLASS_H_JLNQIJLJ
#define CLASS_H_JLNQIJLJ

#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Class;
	typedef const ObjectPtr<Class>& ClassPtr;
	typedef const ObjectPtr<const Class>& ClassConstPtr;
	
	enum MethodType {
		MethodTypeNone,
		MethodTypeFunction,
		MethodTypeProperty
	};
	
	struct Property {
		VALUE getter;
		VALUE setter;
	};
	
	struct Method {
		MethodType type;
		SnSymbol name;
		union {
			VALUE function;
			Property* property;
		};
	};
	
	ObjectPtr<Class> get_class_class();
	bool is_class(VALUE val);
	
	ObjectPtr<Class> create_class(SnSymbol name, ClassPtr super);
	ObjectPtr<Class> create_class_for_type(SnSymbol name, const Type* type);
	ObjectPtr<Class> create_meta_class(ClassPtr cls);
	bool class_is_meta(ClassConstPtr cls);
	const char* class_get_name(ClassConstPtr cls);
	
	// Instance variables API
	int32_t class_get_index_of_instance_variable(ClassConstPtr cls, SnSymbol name);
	int32_t class_define_instance_variable(ClassPtr cls, SnSymbol name);
	size_t class_get_num_instance_variables(ClassConstPtr cls);
	
	// Methods and Properties API
	bool class_lookup_method(ClassConstPtr cls, SnSymbol name, Method* out_method);
	void class_get_method(ClassConstPtr cls, SnSymbol name, Method* out_method); // throws if not found!
	ClassPtr _class_define_method(ClassPtr cls, SnSymbol name, VALUE function);
	ClassPtr _class_define_property(ClassPtr cls, SnSymbol name, VALUE getter, VALUE setter);
	#define SN_DEFINE_METHOD(CLS, NAME, FUNCPTR) snow::_class_define_method(CLS, snow_sym(NAME), snow_create_function(FUNCPTR, snow_sym(#FUNCPTR)))
	#define SN_DEFINE_PROPERTY(CLS, NAME, GETTER, SETTER) snow::_class_define_property(CLS, snow_sym(NAME), snow_create_function(GETTER, snow_sym(#GETTER)), snow_create_function(SETTER, snow_sym(#SETTER)))
	VALUE class_get_initialize(ClassConstPtr cls);
}

#endif /* end of include guard: CLASS_H_JLNQIJLJ */
