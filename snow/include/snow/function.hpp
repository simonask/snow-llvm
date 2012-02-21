#pragma once
#ifndef FUNCTION_H_X576C5TP
#define FUNCTION_H_X576C5TP

#include "snow/basic.h"
#include "snow/symbol.hpp"
#include "snow/value.hpp"
#include "snow/object.hpp"
#include "snow/objectptr.hpp"
#include "snow/arguments.h"

namespace snow {
	struct Class;
	struct Array;
	struct Function;
	struct Environment;
	typedef ObjectPtr<const Environment> EnvironmentConstPtr;
	
	struct MethodCacheLine;
	struct InstanceVariableCacheLine;
	
	struct CallFrame {
		ObjectPtr<Function> function;
		CallFrame* caller;
		Value self;
		Value* locals; // size: function->descriptor.num_locals
		const SnArguments* args;
		ObjectPtr<Environment> environment;
	};
	
	typedef VALUE(*FunctionPtr)(const CallFrame* here, VALUE self, VALUE it);
	
	ObjectPtr<Class> get_function_class();
	ObjectPtr<Class> get_environment_class();
	
	ObjectPtr<Function> create_function(FunctionPtr ptr, Symbol name);
	ObjectPtr<Environment> call_frame_environment(CallFrame* frame);
	Value* get_locals_from_higher_lexical_scope(const CallFrame* frame, size_t num_levels);
	ObjectPtr<Function> environment_get_function(ObjectPtr<const Environment> env);
	Value* environment_get_locals(ObjectPtr<const Environment> env);
	Value environment_get_self(ObjectPtr<const Environment> env);
	ObjectPtr<const Array> environment_get_arguments(ObjectPtr<const Environment> env);
	
	void merge_splat_arguments(CallFrame* callee_context, Value mergee);
	ObjectPtr<Function> value_to_function(Value val, Value* out_new_self);
	
	Value function_call(ObjectPtr<Function> function, CallFrame* frame);
	Symbol function_get_name(ObjectPtr<const Function> function);
	size_t function_get_num_locals(ObjectPtr<const Function> function);
	ObjectPtr<Environment> function_get_definition_scope(ObjectPtr<const Function> function);
	MethodCacheLine* function_get_method_cache_lines(ObjectPtr<const Function> function);
	InstanceVariableCacheLine* function_get_instance_variable_cache_lines(ObjectPtr<const Function> function);
	AnyObjectPtr function_get_module(ObjectPtr<const Function> function);
	
	// Convenience for currying `self`.
	AnyObjectPtr create_method_proxy(Value self, Value method);
}

#endif /* end of include guard: FUNCTION_H_X576C5TP */
