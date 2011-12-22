#pragma once
#ifndef OBJECT_H_CCPHDYB5
#define OBJECT_H_CCPHDYB5

#include "snow/gc.hpp"
#include "snow/value.hpp"
#include "snow/symbol.hpp"
#include "snow/type.hpp"
#include "snow/util.hpp"
#include "snow/objectptr.hpp"

struct SnArguments;

namespace snow {
	ObjectPtr<Class> get_object_class();
	AnyObjectPtr create_object(const Value& cls, size_t num_constructor_args, const Value* args);
	AnyObjectPtr create_object_with_arguments(const ObjectPtr<Class>& cls, const struct SnArguments* constructor_args);
	AnyObjectPtr create_object_without_initialize(const ObjectPtr<Class>& cls);

	// Instance variable related
	Value object_get_instance_variable(const AnyObjectPtr& obj, Symbol name);
	Value object_set_instance_variable(const AnyObjectPtr& obj, Symbol name, const Value& val);
	Value object_get_instance_variable_by_index(const AnyObjectPtr& obj, int32_t idx);
	Value& object_set_instance_variable_by_index(const AnyObjectPtr& obj, int32_t idx, const Value& val);
	int32_t object_get_index_of_instance_variable(const AnyObjectPtr& obj, Symbol name); // -1 if not found
	int32_t object_get_or_create_index_of_instance_variable(const AnyObjectPtr& object, Symbol name);

	// Meta-class related
	bool object_give_meta_class(const AnyObjectPtr& obj);
	Value _object_define_method(const AnyObjectPtr& obj, Symbol name, const Value& func);
	#define object_define_method(OBJ, NAME, FUNC) _object_define_method(OBJ, snow::sym(NAME), snow::create_function(FUNC, snow::sym(#FUNC)))
	Value _object_define_property(const AnyObjectPtr& obj, Symbol name, const Value& getter, const Value& setter);
	#define object_define_property(OBJ, NAME, GETTER, SETTER) _object_define_property(OBJ, snow::sym(NAME), snow::create_function(GETTER, snow::sym(#GETTER)), snow::create_function(SETTER, snow::sym(#SETTER)))
	Value object_set_property_or_define_method(const AnyObjectPtr& self, Symbol name, const Value& val);
}

#endif /* end of include guard: OBJECT_H_CCPHDYB5 */
