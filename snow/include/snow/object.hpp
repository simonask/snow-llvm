#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/gc.hpp"
#include "snow/value.hpp"
#include "snow/symbol.hpp"
#include "snow/type.hpp"
#include "snow/util.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	class Arguments;
	
	ObjectPtr<Class> get_object_class();
	AnyObjectPtr create_object(ObjectPtr<Class> cls, size_t num_constructor_args, const Value* args);
	AnyObjectPtr create_object_with_arguments(ObjectPtr<Class> cls, const Arguments& constructor_args);
	AnyObjectPtr create_object_without_initialize(ObjectPtr<Class> cls);
	void object_initialize(AnyObjectPtr obj, const Arguments& args);

	// Instance variable related
	Value object_get_instance_variable(AnyObjectPtr obj, Symbol name);
	Value object_set_instance_variable(AnyObjectPtr obj, Symbol name, Value val);
	Value object_get_instance_variable_by_index(AnyObjectPtr obj, int32_t idx);
	Value& object_set_instance_variable_by_index(AnyObjectPtr obj, int32_t idx, Value val);
	int32_t object_get_index_of_instance_variable(AnyObjectPtr obj, Symbol name); // -1 if not found
	int32_t object_get_or_create_index_of_instance_variable(AnyObjectPtr object, Symbol name);

	// Meta-class related
	bool object_give_meta_class(AnyObjectPtr obj);
	Value _object_define_method(AnyObjectPtr obj, Symbol name, Value func);
	#define SN_OBJECT_DEFINE_METHOD(OBJ, NAME, FUNC) snow::_object_define_method(OBJ, snow::sym(NAME), snow::create_function(FUNC, snow::sym(#FUNC)))
	Value _object_define_property(AnyObjectPtr obj, Symbol name, Value getter, Value setter);
	#define SN_OBJECT_DEFINE_PROPERTY(OBJ, NAME, GETTER, SETTER) snow::_object_define_property(OBJ, snow::sym(NAME), snow::create_function(GETTER, snow::sym(#GETTER)), snow::create_function(SETTER, snow::sym(#SETTER)))
	Value object_set_property_or_define_method(AnyObjectPtr self, Symbol name, Value val);
}

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
