#include "snow/symbol.h"
#include "internal.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/process.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/type.h"
#include "snow/vm.h"

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

static VALUE symbol_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_is_symbol(self));
	const char* symstr = snow_sym_to_cstr(snow_value_to_symbol(self));
	
	if (!symstr) {
		snow_throw_exception_with_description("'%p' is not in the symbol table.", self);
		return NULL;
	}
	
	size_t len = strlen(symstr);
	char* str = alloca(len+2);
	str[0] = '#';
	memcpy(str+1, symstr, len);
	str[len+1] = '\0';
	return snow_create_string(str);
}

static VALUE symbol_to_string(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_is_symbol(self));
	return snow_create_string_constant(snow_sym_to_cstr(snow_value_to_symbol(self)));
}

SnClass* snow_get_symbol_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("inspect", symbol_inspect, 0),
			SN_METHOD("to_string", symbol_to_string, 0),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Symbol"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}
