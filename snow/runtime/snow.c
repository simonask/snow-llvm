#include "snow/snow.h"
#include "snow/vm.h"
#include "snow/process.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/numeric.h"
#include "snow/boolean.h"
#include "snow/nil.h"
#include "snow/str.h"
#include "snow/parser.h"
#include "snow/vm.h"
#include "snow/function.h"
#include "snow/map.h"
#include "snow/exception.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

void snow_lex(const char*);

struct SnAstNode;

static SnProcess main_process;
static SnObject* immediate_prototypes[8];

static SnObject* get_nearest_object(VALUE);

SnProcess* snow_init(struct SnVM* vm) {
	printf("snow_init()\n");
	main_process.vm = vm;
	return &main_process;;
}

SnProcess* snow_get_process() {
	return &main_process;
}

VALUE snow_eval(const char* source) {
	SnFunction* f = snow_compile(source);
	if (f) {
		printf("calling compiled code...\n");
		return snow_call(f, NULL, 0, NULL);
	}
	fprintf(stderr, "ERROR: Function is NULL.\n");
	return NULL;
}

SnFunction* snow_compile(const char* source) {
	struct SnAST* ast = snow_parse(source);
	if (ast) {
		SnCompilationResult result;
		if (snow_vm_compile_ast(ast, &result)) {
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

SnFunction* snow_compile_file(const char* path) {
	return NULL;
}

VALUE snow_call(VALUE functor, VALUE self, size_t num_args, const VALUE* args) {
	SnFunction* function = snow_value_to_function(functor);
	SnFunctionCallContext* context = snow_create_function_call_context(function, NULL, 0, NULL, num_args, args);
	return snow_function_call(function, context, self, num_args ? args[0] : NULL);
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
	
	SnFunction* function = snow_value_to_function(functor);
	SnFunctionCallContext* context = snow_create_function_call_context(function, NULL, num_names, names, num_args, args);
	return snow_function_call(function, context, self, it);
}

SnFunction* snow_get_method(VALUE object, SnSymbol member) {
	printf("snow_get_method: %p.%s\n", object, snow_sym_to_cstr(member));
	// TODO: Support functor objects
	VALUE f = snow_get_member(object, member);
	if (!f) {
		snow_throw_exception_with_description("Member '%s' of object %p does not exist.", snow_sym_to_cstr(member), object);
		return NULL;
	}
	if (snow_type_of(f) != SnFunctionType) {
		snow_throw_exception_with_description("Method '%s' of object %p is not a function.", snow_sym_to_cstr(member), object);
		return NULL;
	}
	return (SnFunction*)f;
}

VALUE snow_get_member(VALUE self, SnSymbol member) {
	return NULL;
}

VALUE snow_set_member(VALUE self, SnSymbol member, VALUE val) {
	return NULL;
}

static SnMap** get_global_storage() {
	static SnMap* a = NULL;
	if (!a) {
		a = snow_create_map();
	}
	return &a;
}

VALUE snow_get_global(SnSymbol sym) {
	SnMap* storage = *get_global_storage();
	return snow_map_get(storage, snow_symbol_to_value(sym));
}

void snow_set_global(SnSymbol sym, VALUE val) {
	SnMap* storage = *get_global_storage();
	snow_map_set(storage, snow_symbol_to_value(sym), val);
}

VALUE snow_require(const char* file) {
	printf("(requiring file %s)\n", file);
	return SN_NIL;
}

void snow_printf(const char* fmt, size_t num_args, ...) {
	va_list ap;
	va_start(ap, num_args);
	snow_vprintf(fmt, num_args, ap);
	va_end(ap);
}

void snow_vprintf(const char* fmt, size_t num_args, va_list ap) {
//	VALUE vstrs[num_args];
	for (size_t i = 0; i < num_args; ++i) {
	}
	printf("%s", fmt);
}


SnObject* get_nearest_object(VALUE val) {
	while (!snow_is_object(val)) {
		val = snow_get_member(val, snow_sym("__prototype__"));
	}
	return (SnObject*)val;
}

bool snow_eval_truth(VALUE val) {
	return val != NULL && val != SN_NIL && val != SN_FALSE;
}