#include "snow/symbol.h"
#include "snow/type.h"
#include "snow/snow.h"
#include "snow/process.h"
#include "snow/vm.h"

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
