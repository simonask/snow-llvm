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
	SnValueType get_type(VALUE v);
	SnObject* get_global_object();
	SnObject** _get_global_object_ptr(); // for GC
	VALUE set_global(SnSymbol sym, VALUE val);
	VALUE get_global(SnSymbol sym);
	VALUE local_missing(CallFrame* frame, SnSymbol name);
	
	ObjectPtr<Class> get_class(VALUE value);
	VALUE call(VALUE functor, VALUE self, size_t num_args, VALUE* args);
	VALUE call_with_arguments(VALUE functor, VALUE self, const SnArguments* args);
	VALUE call_method(VALUE self, SnSymbol method_name, size_t num_args, VALUE* args);
	VALUE call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args);
	VALUE call_method_with_named_arguments(VALUE self, SnSymbol method, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args);
	#define SN_CALL_METHOD(SELF, NAME, NUM_ARGS, ARGS) snow::call_method(SELF, snow_sym(NAME), NUM_ARGS, ARGS)
	VALUE get_method(VALUE self, SnSymbol method);
	
	VALUE value_freeze(VALUE it);
	
	ObjectPtr<String> value_to_string(VALUE val);
	ObjectPtr<String> value_inspect(VALUE val);
}

#endif /* end of include guard: SNOW_H_ILWGKE1Z */
