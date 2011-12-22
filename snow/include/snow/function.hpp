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
	struct Function;
	struct Environment;
	typedef const ObjectPtr<const Environment>& EnvironmentConstPtr;
	
	struct CallFrame {
		ObjectPtr<Function> function;
		CallFrame* caller;
		Value self;
		Value* locals; // size: function->descriptor.num_locals
		const SnArguments* args;
		ObjectPtr<Environment> environment;
	};
	
	typedef Value(*FunctionPtr)(const CallFrame* here, const Value& self, const Value& it);
	
	ObjectPtr<Class> get_function_class();
	ObjectPtr<Class> get_environment_class();
	
	ObjectPtr<Function> create_function(FunctionPtr ptr, Symbol name);
	ObjectPtr<Environment> call_frame_environment(CallFrame* frame);
	Value* get_locals_from_higher_lexical_scope(const CallFrame* frame, size_t num_levels);
	ObjectPtr<Function> environment_get_function(const ObjectPtr<const Environment>& env);
	Value* environment_get_locals(const ObjectPtr<const Environment>& env);
	
	void merge_splat_arguments(CallFrame* callee_context, Value mergee);
	ObjectPtr<Function> value_to_function(Value val, Value* out_new_self);
	
	Value function_call(const ObjectPtr<Function>& function, CallFrame* frame);
	Symbol function_get_name(const ObjectPtr<const Function>& function);
	size_t function_get_num_locals(const ObjectPtr<const Function>& function);
	ObjectPtr<Environment> function_get_definition_scope(const ObjectPtr<const Function>& function);
	
	// Convenience for currying `self`.
	Value create_method_proxy(const Value& self, const Value& method);
}

#endif /* end of include guard: FUNCTION_H_X576C5TP */
