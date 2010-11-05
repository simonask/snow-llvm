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
#include "snow/arguments.h"
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
		SnArguments* args;
		SN_ALLOC_ARGUMENTS(args, 0, f);
		printf("calling compiled code...\n");
		return snow_call(f, NULL, args);
	}
	fprintf(stderr, "ERROR: Function is NULL.\n");
	return NULL;
}

SnFunction* snow_compile(const char* source) {
	printf("parsing...\n");
	struct SnAST* ast = snow_parse(source);
	if (ast) {
		SnCompilationResult result;
		if (snow_vm_compile_ast(ast, &result)) {
			return NULL; // TODO! Make a real function
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

VALUE snow_call(VALUE functor, VALUE self, struct SnArguments* args) {
	VALUE f = functor;
	while (snow_type_of(f) != SnFunctionType) {
		f = snow_get_member(f, snow_sym("__call__"));
	}
	
	SnFunction* function = (SnFunction*)f;
	return SN_NIL;//snow_function_call(function, self, args);
}

SnFunction* snow_get_method(VALUE object, SnSymbol member) {
	// TODO: Support functor objects
	VALUE f = snow_get_member(object, member);
	if (snow_type_of(f) != SnFunctionType) {
		snow_throw_exception_with_description("Method %s not found on object %p.", snow_sym_to_cstr(member), object);
		return NULL;
	}
	return (SnFunction*)f;
}

VALUE snow_call_method_va(VALUE self, SnSymbol member, size_t num_args, ...) {
	SnFunction* method = snow_get_method(self, member);
	SnArguments* args;
	SN_ALLOC_ARGUMENTS(args, num_args, method);
	va_list ap;
	va_start(ap, num_args);
	for (size_t i = 0; i < num_args; ++i) {
		snow_arguments_push(args, va_arg(ap, VALUE));
	}
	va_end(ap);
	return snow_call_method(self, method, args);
}

VALUE snow_call_method(VALUE self, SnFunction* function, SnArguments* args) {
	return SN_NIL;//snow_function_call(self, function, args);
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