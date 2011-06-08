#pragma once
#ifndef FUNCTION_INTERNAL_HPP_U4QUBRPT
#define FUNCTION_INTERNAL_HPP_U4QUBRPT

#include "snow/function.h"
#include "snow/symbol.h"
#include "snow/value.h"

namespace snow {
	struct VariableReference {
		int level; // The number of call scopes to go up to find the variable.
		int index; // The index of the variable in that scope.
		
		bool operator==(const VariableReference& other) const { return level == other.level && index == other.index; }
	};
	
	struct FunctionDescriptor {
		SnFunctionPtr ptr;
		SnSymbol name;
		SnValueType return_type;
		size_t num_params;
		SnValueType* param_types;
		SnSymbol* param_names;
		SnSymbol* local_names;
		uint32_t num_locals; // num_locals >= num_params (locals include arguments)
		size_t num_variable_references;
		VariableReference* variable_references;
	};
	
	SnObject* create_function_for_descriptor(const FunctionDescriptor* descriptor, SnObject* definition_frame);
}

#endif /* end of include guard: FUNCTION_INTERNAL_HPP_U4QUBRPT */
