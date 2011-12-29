#pragma once
#ifndef FUNCTION_INTERNAL_HPP_U4QUBRPT
#define FUNCTION_INTERNAL_HPP_U4QUBRPT

#include "snow/function.hpp"
#include "snow/symbol.hpp"
#include "snow/value.hpp"

namespace snow {
	struct VariableReference {
		int level; // The number of call scopes to go up to find the variable.
		int index; // The index of the variable in that scope.
		
		bool operator==(const VariableReference& other) const { return level == other.level && index == other.index; }
	};
	
	struct FunctionDescriptor {
		FunctionPtr ptr;
		Symbol name;
		ValueType return_type;
		size_t num_params;
		ValueType* param_types;
		Symbol* param_names;
		Symbol* local_names;
		uint32_t num_locals; // num_locals >= num_params (locals include arguments)
		size_t num_variable_references;
		VariableReference* variable_references;
		
		// for inline cache management
		size_t num_method_calls;
		size_t num_instance_variable_accesses;
	};
	
	ObjectPtr<Function> create_function_for_descriptor(const FunctionDescriptor* descriptor, ObjectPtr<Environment> definition_frame);
}

#endif /* end of include guard: FUNCTION_INTERNAL_HPP_U4QUBRPT */
