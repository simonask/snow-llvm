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

#include <stdarg.h>
#include <string.h>

void snow_lex(SN_P, const char*);

struct SnAstNode;

static SnProcess main_process;
static SnObject* immediate_prototypes[8];

static SnObject* get_nearest_object(SN_P, VALUE);

SnProcess* snow_init(struct SnVM* vm) {
	printf("snow_init()\n");
	main_process.vm = vm;
	main_process.immediate_prototypes = immediate_prototypes;
	memset(immediate_prototypes, 0, sizeof(immediate_prototypes));
	snow_vm_init(&main_process);
	return &main_process;;
}

SnProcess* snow_get_main_process() {
	return &main_process;
}

const SnType* snow_get_type(VALUE v) {
	switch (snow_type_of(v)) {
		case SnPointerType: return ((SnObject*)v)->type;
		case SnNilType: return snow_get_nil_type();
		case SnIntegerType: return snow_get_integer_type();
		case SnFalseType: case SnTrueType: return snow_get_boolean_type();
		case SnSymbolType: return snow_get_symbol_type();
		case SnFloatType: return snow_get_float_type();
		default: {
			TRAP(); // ERROR: Unknown type?!?!
			return NULL;
		}
	}
}

VALUE snow_eval(SN_P p, const char* source) {
	VALUE f = snow_compile(p, source);
	if (f) {
		printf("calling compiled code...\n");
		return snow_call(p, f, NULL);
	}
	fprintf(stderr, "ERROR: Function is NULL.\n");
	return NULL;
}

VALUE snow_compile(SN_P p, const char* source) {
	printf("parsing...\n");
	snow_parse(p, source);
	return NULL;
/*	SnParserInfo info;
	struct SnAstNode* ast = snow_parse(p, source, &info);
	if (!ast) return NULL;
	printf("compiling...\n");
	return snow_vm_compile_ast(p, ast, source).obj;*/
}

VALUE snow_compile_file(SN_P p, const char* path) {
	return NULL;
}

VALUE snow_call(SN_P p, VALUE object, struct SnArguments* args) {
	return snow_call_with_self(p, object, NULL, args);
}

VALUE snow_call_with_self(SN_P p, VALUE object, VALUE self, struct SnArguments* args) {
	VALUE functor = object;
	while (!snow_is_object(functor)) {
		functor = snow_get_member(p, functor, SN_SYM("__call__"));
	}
	
	SnObject* f = (SnObject*)functor;
	return f->type->call(p, f, self, args);
}

VALUE snow_call_method(SN_P p, VALUE self, SnSymbol member, struct SnArguments* args) {
	VALUE method = snow_get_member(p, self, member);
	if (NULL == method) {
		fprintf(stderr, "ERROR: Method '%s' not found on object.\n", snow_sym_to_cstr(p, member));
		TRAP(); // TODO: Raise exception
		return NULL;
	}
	return snow_call_with_self(p, method, self, args);
}

VALUE snow_get_member(SN_P p, VALUE self, SnSymbol member) {
	const SnType* type = snow_get_type(self);
	ASSERT(type != NULL);
	return type->get_member(p, self, member);
}

VALUE snow_set_member(SN_P p, VALUE self, SnSymbol member, VALUE val) {
	const SnType* type = snow_get_type(self);
	ASSERT(type != NULL);
	return type->set_member(p, self, member, val);
}

void snow_modify(SN_P p, SnObject* object) {
	if (object && object->owner != p) {
		TRAP(); // modified object owned by another process!
	}
}

static SnArrayRef get_global_storage(SN_P p) {
	static SnObject* a = NULL;
	if (!a) {
		a = snow_create_array(p).obj;
	}
	return snow_object_as_array(a);
}

VALUE snow_get_global(SN_P p, SnSymbol sym) {
	return NULL;
}

void snow_set_global(SN_P p, SnSymbol sym, VALUE val) {
	
}

VALUE snow_require(SN_P p, const char* file) {
	printf("(requiring file %s)\n", file);
	return SN_NIL;
}

void snow_printf(SN_P p, const char* fmt, size_t num_args, ...) {
	va_list ap;
	va_start(ap, num_args);
	snow_vprintf(p, fmt, num_args, ap);
	va_end(ap);
}

void snow_vprintf(SN_P p, const char* fmt, size_t num_args, va_list ap) {
//	VALUE vstrs[num_args];
	for (size_t i = 0; i < num_args; ++i) {
	}
	printf("%s", fmt);
}


SnObject* get_nearest_object(SN_P p, VALUE val) {
	while (!snow_is_object(val)) {
		val = snow_get_member(p, val, SN_SYM("__prototype__"));
	}
	return (SnObject*)val;
}