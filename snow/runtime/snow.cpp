#include "snow/snow.hpp"
#include "internal.h"
#include "globals.h"
#include "snow/array.hpp"
#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/exception.h"
#include "snow/fiber.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "snow/map.hpp"
#include "snow/module.hpp"
#include "snow/nil.h"
#include "snow/numeric.hpp"
#include "snow/object.hpp"
#include "snow/parser.h"
#include "snow/str.hpp"
#include "snow/vm.h"
#include "snow/vm.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

using namespace snow;

CAPI {
	void snow_lex(const char*);
	void snow_init_fibers();
}

struct SnAstNode;

namespace snow {
	void init(const char* lib_path) {
		const void* stk;
		snow_init_gc(&stk);
		snow_init_fibers();
		snow_init_globals();
		load_in_global_module(snow::create_string_constant("lib/prelude.sn"));
	}

	void finish() {}

	const char* version() {
		return "0.0.1 pre-alpha [x86-64]";
	}

	VALUE get_global(SnSymbol name) {
		SnObject* go = get_global_module();
		return snow_object_get_instance_variable(go, name);
	}

	VALUE set_global(SnSymbol name, VALUE val) {
		SnObject* go = get_global_module();
		return snow_object_set_instance_variable(go, name, val);
	}

	VALUE local_missing(CallFrame* frame, SnSymbol name) {
		// XXX: TODO!!
		return get_global(name);
		//fprintf(stderr, "LOCAL MISSING: %s\n", snow_sym_to_cstr(name));
		//return NULL;
	}

	ObjectPtr<Class> get_immediate_class(SnValueType type) {
		ASSERT(type != SnObjectType);
		switch (type) {
			case SnIntegerType: return snow_get_integer_class();
			case SnNilType: return snow_get_nil_class();
			case SnBooleanType: return snow_get_boolean_class();
			case SnSymbolType: return snow_get_symbol_class();
			case SnFloatType: return snow_get_float_class();
			default: TRAP(); return NULL;
		}
	}

	ObjectPtr<Class> get_class(VALUE value) {
		if (snow_is_object(value)) {
			SnObject* object = (SnObject*)value;
			return object->cls ? object->cls : snow_get_object_class();
		} else {
			return get_immediate_class(snow_type_of(value));
		}
	}

	VALUE call(VALUE functor, VALUE self, size_t num_args, VALUE* args) {
		SnArguments arguments = {
			.size = num_args,
			.data = args,
		};
		return call_with_arguments(functor, self, &arguments);
	}

	VALUE call_with_arguments(VALUE functor, VALUE self, const SnArguments* args) {
		VALUE new_self = self;
		SnObject* function = value_to_function(functor, &new_self);
		CallFrame frame = {
			.self = new_self,
			.args = args,
		};
		return function_call(function, &frame);
	}

	VALUE call_method(VALUE self, SnSymbol method_name, size_t num_args, VALUE* args) {
		ObjectPtr<Class> cls = get_class(self);
		Method method;
		class_get_method(cls, method_name, &method);
		VALUE func = NULL;
		if (method.type == MethodTypeFunction) {
			func = method.function;
		} else if (method.type == MethodTypeProperty) {
			func = call(method.property->getter, self, 0, NULL);
		}
		return call(func, self, num_args, args);
	}

	VALUE call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
		ASSERT(num_names <= num_args);
		SnArguments arguments = {
			.num_names = num_names,
			.names = names,
			.size = num_args,
			.data = args,
		};
		return call_with_arguments(functor, self, &arguments);
	}

	VALUE value_freeze(VALUE it) {
		// TODO!!
		return it;
	}

	VALUE get_module_value(SnObject* module, SnSymbol member) {
		VALUE v = snow_object_get_instance_variable(module, member);
		if (v) return v;
		snow_throw_exception_with_description("Variable '%s' not found in module %p.", snow_sym_to_cstr(member), module);
		return NULL;
	}


	ObjectPtr<String> value_to_string(VALUE it) {
		ObjectPtr<String> str = SN_CALL_METHOD(it, "to_string", 0, NULL);
		ASSERT(str != NULL); // .to_string returned non-String
		return str;
	}

	ObjectPtr<String> value_inspect(VALUE it) {
	 	ObjectPtr<String> str = SN_CALL_METHOD(it, "inspect", 0, NULL);
		ASSERT(str != NULL); // .inspect returned non-String
		return str;
	}
}