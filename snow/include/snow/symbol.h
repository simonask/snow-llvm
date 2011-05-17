#pragma once
#ifndef SYMBOL_H_RKFL2KE8
#define SYMBOL_H_RKFL2KE8

#include "snow/basic.h"
#include "snow/value.h"

typedef uint64_t SnSymbol;

CAPI SnSymbol snow_sym(const char* str);
CAPI const char* snow_sym_to_cstr(SnSymbol sym);

INLINE bool snow_is_symbol(VALUE val) {
	return ((uintptr_t)val & SnTypeMask) == SnSymbolType;
}

INLINE VALUE snow_symbol_to_value(SnSymbol sym) {
	return (VALUE)(sym << 4 | SnSymbolType);
}

INLINE SnSymbol snow_value_to_symbol(VALUE val) {
	ASSERT(snow_is_symbol(val));
	return (SnSymbol)((uintptr_t)val >> 4);
}

INLINE VALUE snow_vsym(const char* str) {
	return snow_symbol_to_value(snow_sym(str));
}

struct SnClass;
CAPI struct SnClass* snow_get_symbol_class();

#endif /* end of include guard: SYMBOL_H_RKFL2KE8 */
