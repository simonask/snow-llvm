#include "snow/snow.hpp"
#include "internal.h"
#include "globals.h"
#include "snow/array.hpp"
#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/exception.hpp"
#include "snow/fiber.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "snow/map.hpp"
#include "snow/module.hpp"
#include "snow/nil.hpp"
#include "snow/numeric.hpp"
#include "snow/object.hpp"
#include "snow/parser.hpp"
#include "snow/str.hpp"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

using namespace snow;

CAPI {
	void snow_lex(const char*);
}

struct ASTNode;

namespace snow {
	void init_fibers();
	
	void init(const char* lib_path) {
		void* stk;
		init_gc(&stk);
		init_fibers();
		snow_init_globals();
		load_in_global_module(snow::create_string_constant("lib/prelude.sn"));
	}

	void finish() {}

	const char* version() {
		return "0.0.1 pre-alpha [x86-64]";
	}

	Value get_global(Symbol name) {
		AnyObjectPtr go = get_global_module();
		return object_get_instance_variable(go, name);
	}

	Value set_global(Symbol name, Value val) {
		AnyObjectPtr go = get_global_module();
		return object_set_instance_variable(go, name, val);
	}

	Value local_missing(CallFrame* frame, Symbol name) {
		// XXX: TODO!!
		Value v = get_global(name);
		if (v == NULL) {
			throw_exception_with_description("LOCAL MISSING: %@\n", sym_to_cstr(name));
		}
		return v;
	}

	ObjectPtr<Class> get_immediate_class(ValueType type) {
		ASSERT(type != ObjectType);
		switch (type) {
			case IntegerType: return get_integer_class();
			case NilType: return get_nil_class();
			case BooleanType: return get_boolean_class();
			case SymbolType: return get_symbol_class();
			case FloatType: return get_float_class();
			default: TRAP(); return NULL;
		}
	}

	ObjectPtr<Class> get_class(Value value) {
		if (is_object(value)) {
			AnyObjectPtr object = value;
			return object->cls != NULL ? object->cls : get_object_class();
		} else {
			return get_immediate_class(type_of(value));
		}
	}

	Value call(Value functor, Value self, size_t num_args, const Value* args) {
		SnArguments arguments = {
			.size = num_args,
			.data = args,
		};
		return call_with_arguments(functor, self, &arguments);
	}

	Value call_with_arguments(Value functor, Value self, const SnArguments* args) {
		Value new_self = self;
		ObjectPtr<Function> function = value_to_function(functor, &new_self);
		CallFrame frame = {
			.self = new_self,
			.args = args,
		};
		return function_call(function, &frame);
	}

	Value call_method(Value self, Symbol method_name, size_t num_args, const Value* args) {
		ObjectPtr<Class> cls = get_class(self);
		MethodQueryResult method;
		class_get_method(cls, method_name, &method);
		Value func;
		if (method.type == MethodTypeFunction) {
			func = method.result;
		} else if (method.type == MethodTypeProperty) {
			func = call(method.result, self, 0, NULL);
		}
		return call(func, self, num_args, args);
	}

	Value call_with_named_arguments(Value functor, Value self, size_t num_names, const Symbol* names, size_t num_args, const Value* args) {
		ASSERT(num_names <= num_args);
		SnArguments arguments = {
			.num_names = num_names,
			.names = names,
			.size = num_args,
			.data = args,
		};
		return call_with_arguments(functor, self, &arguments);
	}

	Value value_freeze(Value it) {
		// TODO!!
		return it;
	}

	Value get_module_value(AnyObjectPtr module, Symbol member) {
		Value v = object_get_instance_variable(module, member);
		if (v) return v;
		throw_exception_with_description("Variable '%@' not found in module %@.", snow::sym_to_cstr(member), value_inspect(module));
		return NULL;
	}


	ObjectPtr<String> value_to_string(Value it) {
		ObjectPtr<String> str = SN_CALL_METHOD(it, "to_string", 0, NULL);
		ASSERT(str != NULL); // .to_string returned non-String
		return str;
	}

	ObjectPtr<String> value_inspect(Value it) {
	 	ObjectPtr<String> str = SN_CALL_METHOD(it, "inspect", 0, NULL);
		ASSERT(str != NULL); // .inspect returned non-String
		return str;
	}
}