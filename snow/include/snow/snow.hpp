#pragma once
#ifndef SNOW_H_ILWGKE1Z
#define SNOW_H_ILWGKE1Z

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/symbol.hpp"
#include "snow/objectptr.hpp"

struct SnArguments;
struct SnObject;

namespace snow {
	struct CallFrame;
	struct Class;
	struct String;
	
	void init(const char* lib_path);
	void finish();
	int main(int argc, char* const* argv);
	
	const char* version();
	ValueType get_type(const Value& v);
	SnObject* get_global_object();
	SnObject** _get_global_object_ptr(); // for GC
	Value set_global(Symbol sym, const Value& val);
	Value get_global(Symbol sym);
	Value local_missing(CallFrame* frame, Symbol name);
	
	ObjectPtr<Class> get_class(const Value& value);
	Value call(const Value& functor, const Value& self, size_t num_args, const Value* args);
	Value call_with_arguments(const Value& functor, const Value& self, const SnArguments* args);
	Value call_method(const Value& self, Symbol method_name, size_t num_args, const Value* args);
	Value call_with_named_arguments(const Value& functor, const Value& self, size_t num_names, const Symbol* names, size_t num_args, const Value* args);
	Value call_method_with_named_arguments(const Value& self, Symbol method, size_t num_names, const Symbol* names, size_t num_args, const Value* args);
	#define SN_CALL_METHOD(SELF, NAME, NUM_ARGS, ARGS) snow::call_method(SELF, snow::sym(NAME), NUM_ARGS, ARGS)
	const Value& get_method(const Value& self, Symbol method);
	
	const Value& value_freeze(const Value& it);
	
	ObjectPtr<String> value_to_string(const Value& val);
	ObjectPtr<String> value_inspect(const Value& val);
}

#endif /* end of include guard: SNOW_H_ILWGKE1Z */
