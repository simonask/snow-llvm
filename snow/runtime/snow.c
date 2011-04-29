#include "snow/snow.h"
#include "internal.h"
#include "globals.h"
#include "snow/array.h"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/fiber.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/map.h"
#include "snow/module.h"
#include "snow/nil.h"
#include "snow/numeric.h"
#include "snow/object.h"
#include "snow/parser.h"
#include "snow/pointer.h"
#include "snow/process.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/vm.h"
#include "snow/vm.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

void snow_lex(const char*);
void snow_init_fibers();

struct SnAstNode;

static SnProcess main_process;
static SnObject* immediate_prototypes[8];

SnProcess* snow_init(struct SnVM* vm) {
	main_process.vm = vm;
	
	const void* stk;
	snow_init_gc(&stk);
	snow_init_fibers();
	snow_init_globals();
	
	snow_load_in_global_module("lib/prelude.sn");
	
	return &main_process;
}

const char* snow_version() {
	return "0.0.1 pre-alpha [LLVM]";
}

SnProcess* snow_get_process() {
	return &main_process;
}

VALUE snow_get_global(SnSymbol name) {
	SnObject* go = snow_get_global_module();
	return snow_get_member(go, name);
}

VALUE snow_set_global(SnSymbol name, VALUE val) {
	SnObject* go = snow_get_global_module();
	return snow_set_member(go, name, val);
}

SnFunction* snow_compile(const char* module_name, const char* source) {
	struct SnAST* ast = snow_parse(source);
	if (ast) {
		SnCompilationResult result;
		if (snow_vm_compile_ast(module_name, source, ast, &result)) {
			return snow_create_function(result.entry_descriptor, NULL);
		} else if (result.error_str) {
			fprintf(stderr, "ERROR COMPILING FUNCTION: %s\n", result.error_str);
			free(result.error_str);
			return NULL;
		} else {
			fprintf(stderr, "ERROR COMPILING FUNCTION: <UNKNOWN>\n");
			return NULL;
		}
	}
	return NULL;
}

static SnClass* class_for_internal_type(SnType type) {
	switch (type) {
		case SnIntegerType:   return snow_get_integer_class();
		case SnNilType:       return snow_get_nil_class();
		case SnFalseType:
		case SnTrueType:      return snow_get_boolean_class();
		case SnSymbolType:    return snow_get_symbol_class();
		case SnFloatType:     return snow_get_float_class();
		
		case SnObjectType:    return snow_get_object_class();
		case SnClassType:     return snow_get_class_class();
		case SnArrayType:     return snow_get_array_class();
		case SnMapType:       return snow_get_map_class();
		case SnFunctionType:  return snow_get_function_class();
		case SnCallFrameType: return snow_get_call_frame_class();
		case SnArgumentsType: return snow_get_arguments_class();
		case SnPointerType:   return snow_get_pointer_class();
		case SnFiberType:     return snow_get_fiber_class();
		default: {
			ASSERT(false && "Unknown type!");
		}
	}
	return NULL;
}

SnClass* snow_get_class(VALUE value) {
	if (snow_is_object(value)) {
		SnObject* object = (SnObject*)value;
		return object->cls ? object->cls : class_for_internal_type(object->type);
	}
	return class_for_internal_type(snow_type_of(value));
}

VALUE snow_call(VALUE functor, VALUE self, size_t num_args, const VALUE* args) {
	SnFunction* function = snow_value_to_function(functor, &self);
	SnCallFrame* context = snow_create_call_frame(function, 0, NULL, num_args, args);
	return snow_function_call(function, context, self, num_args ? args[0] : NULL);
}

VALUE snow_call_method(VALUE self, SnSymbol method_name, size_t num_args, const VALUE* args) {
	SnFunction* method = snow_get_method(self, method_name, &self);
	if (!method) return NULL;
	ASSERT(snow_type_of(method) == SnFunctionType);
	return snow_call(method, self, num_args, args);
}

struct named_arg { SnSymbol name; VALUE value; };

static int named_arg_cmp(const void* _a, const void* _b) {
	const struct named_arg* a = (struct named_arg*)_a;
	const struct named_arg* b = (struct named_arg*)_b;
	return (int)((int64_t)b->name - (int64_t)a->name);
}

VALUE snow_call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
	ASSERT(num_names <= num_args);
	VALUE it = num_args ? args[0] : NULL;
	if (num_names) {
		// TODO: Sort names in-place.
		struct named_arg* tuples = alloca(sizeof(struct named_arg)*num_names);
		for (size_t i = 0; i < num_names; ++i) { tuples[i].name = names[i]; tuples[i].value = args[i]; }
		qsort(tuples, num_names, sizeof(struct named_arg), named_arg_cmp);
		for (size_t i = 0; i < num_names; ++i) { names[i] = tuples[i].name; args[i] = tuples[i].value; }
		it = args[0];
	}
	
	SnFunction* function = snow_value_to_function(functor, &self);
	SnCallFrame* context = snow_create_call_frame(function, num_names, names, num_args, args);
	return snow_function_call(function, context, self, it);
}

VALUE snow_call_method_with_named_arguments(VALUE self, SnSymbol method_name, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args) {
	SnFunction* method = snow_get_method(self, method_name, &self);
	return snow_call_with_named_arguments(method, self, num_names, names, num_args, args);
}

SnFunction* snow_get_method(VALUE object, SnSymbol member, VALUE* new_self) {
	SnClass* cls = snow_get_class(object);
	const SnMethod* method = snow_find_and_resolve_method(cls, member);
	if (!method) {
		snow_throw_exception_with_description("Object %p does not respond to method '%s'.", object, snow_sym_to_cstr(member));
		return NULL;
	}
	
	VALUE f;
	if (method->type == SnMethodTypeProperty) {
		if (!method->property.getter) {
			snow_throw_exception_with_description("Property '%s' is read-only on object %p.", snow_sym_to_cstr(member), object);
			return NULL;
		}
		f = snow_call(method->property.getter, object, 0, NULL);
	} else {
		f = method->function;
	}
	
	*new_self = object;
	return snow_value_to_function(f, new_self);
}

VALUE snow_get_member(VALUE self, SnSymbol member) {
	if (snow_is_object(self)) {
		SnObject* object = (SnObject*)self;
		SnClass* cls = snow_get_class(self);
		ssize_t idx = snow_class_index_of_field(cls, member, SnFieldNoFlags);
		if (idx < 0) return NULL;
		if (idx >= object->num_members) return NULL;
		return object->members[idx];
	}
	return NULL;
}

VALUE snow_set_member(VALUE self, SnSymbol member, VALUE val) {
	if (snow_is_object(self)) {
		SnObject* object = (SnObject*)self;
		SnClass* cls = snow_get_class(self);
		size_t idx = snow_class_define_field(cls, member, SnFieldNoFlags);
		ASSERT(idx < cls->num_fields);
		if (object->num_members < idx+1) {
			size_t old_size = object->num_members;
			object->members = realloc(object->members, sizeof(VALUE) * cls->num_fields);
			for (size_t i = old_size; i < cls->num_fields; ++i) {
				object->members[i] = NULL;
			}
			object->num_members = cls->num_fields;
		}
		object->members[idx] = val;
		return val;
	}
	
	snow_throw_exception_with_description("Immediate of type %d cannot have member variables.", snow_type_of(val));
	return NULL;
}

VALUE snow_value_freeze(VALUE it) {
	// TODO!!
	return it;
}

VALUE snow_get_module_value(SnObject* module, SnSymbol member) {
	VALUE v = snow_get_member(module, member);
	if (v) return v;
	snow_throw_exception_with_description("Variable '%s' not found in module '%s'.", snow_sym_to_cstr(member), snow_value_inspect_cstr(module));
	return NULL;
}


SnString* snow_value_to_string(VALUE it) {
	VALUE vstr = SNOW_CALL_METHOD(it, "to_string", 0, NULL);
	ASSERT(snow_type_of(vstr) == SnStringType);
	return (SnString*)vstr;
}

SnString* snow_value_inspect(VALUE it) {
	VALUE vstr = SNOW_CALL_METHOD(it, "inspect", 0, NULL);
	ASSERT(snow_type_of(vstr) == SnStringType);
	return (SnString*)vstr;
}

const char* snow_value_to_cstr(VALUE it) {
	return snow_string_cstr(snow_value_to_string(it));
}

const char* snow_value_inspect_cstr(VALUE it) {
	return snow_string_cstr(snow_value_inspect(it));
}
