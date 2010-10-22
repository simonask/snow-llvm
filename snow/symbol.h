#pragma once
#ifndef SYMBOL_H_RKFL2KE8
#define SYMBOL_H_RKFL2KE8

#include "snow/basic.h"
#include "snow/value.h"

struct SnType;
struct SnObject;
CAPI const struct SnType* snow_get_symbol_type();
CAPI struct SnObject* snow_get_symbol_prototype(SN_P);

typedef uint64_t SnSymbol;

CAPI VALUE snow_vsym(SN_P, const char* str);
CAPI SnSymbol snow_sym(SN_P, const char* str);
CAPI const char* snow_sym_to_cstr(SN_P, SnSymbol sym);

// convenience
#define SN_SYM(str) snow_sym(p, (str))
#define SN_VSYM(str) snow_vsym(p, (str))

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
