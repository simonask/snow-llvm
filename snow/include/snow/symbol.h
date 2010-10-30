#pragma once
#ifndef SYMBOL_H_RKFL2KE8
#define SYMBOL_H_RKFL2KE8

#include "snow/basic.h"
#include "snow/value.h"

typedef uint64_t SnSymbol;

CAPI VALUE snow_vsym(const char* str);
CAPI SnSymbol snow_sym(const char* str);
CAPI const char* snow_sym_to_cstr(SnSymbol sym);

static inline bool snow_is_symbol(VALUE val) {
	return ((uintptr_t)val & SnTypeMask) == SnSymbolType;
}

static inline VALUE snow_symbol_to_value(SnSymbol sym) {
	return (VALUE)(sym << 4 | SnSymbolType);
}

static inline SnSymbol snow_value_to_symbol(VALUE val) {
	ASSERT(snow_is_symbol(val));
	return (SnSymbol)((uintptr_t)val >> 4);
}

#endif /* end of include guard: SYMBOL_H_RKFL2KE8 */
