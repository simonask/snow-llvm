#include "snow/symbol.h"
#include "snow/type.h"
#include "snow/snow.h"
#include "snow/process.h"
#include "snow/vm.h"
#include "snow/function.h"
#include "snow/str.h"

#include <string.h>
#include <alloca.h>

VALUE snow_vsym(const char* str) {
	return snow_symbol_to_value(snow_sym(str));
}

SnSymbol snow_sym(const char* cstr) {
	SnProcess* p = snow_get_process();
	return p->vm->symbol(cstr);
}

const char* snow_sym_to_cstr(SnSymbol sym) {
	SnProcess* p = snow_get_process();
	return p->vm->symbol_to_cstr(sym);
}

static VALUE symbol_inspect(SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_is_symbol(self));
	const char* symstr = snow_sym_to_cstr(snow_value_to_symbol(self));
	
	if (!symstr) {
		fprintf(stderr, "ERROR: %p is not in the symbol table.\n", self);
		return SN_NIL;
	}
	
	size_t len = strlen(symstr);
	char* str = alloca(len+2);
	str[0] = '#';
	memcpy(str+1, symstr, len);
	str[len+1] = '\0';
	return snow_create_string(str);
}

static VALUE symbol_to_string(SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_is_symbol(self));
	return snow_create_string_constant(snow_sym_to_cstr(snow_value_to_symbol(self)));
}

SnObject* snow_create_symbol_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", symbol_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", symbol_to_string, 0);
	return proto;
}
