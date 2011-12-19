#pragma once
#ifndef SYMBOL_H_RKFL2KE8
#define SYMBOL_H_RKFL2KE8

#include "snow/basic.h"
#include "snow/value.hpp"

namespace snow {
	typedef uint64_t Symbol;

	Symbol sym(const char* str);
	const char* sym_to_cstr(Symbol sym);

	INLINE bool is_symbol(VALUE val) {
		return ((uintptr_t)val & ValueTypeMask) == SymbolType;
	}

	INLINE VALUE symbol_to_value(Symbol sym) {
		return (VALUE)(sym << 4 | SymbolType);
	}

	INLINE Symbol value_to_symbol(VALUE val) {
		ASSERT(is_symbol(val));
		return (Symbol)((uintptr_t)val >> 4);
	}

	INLINE VALUE vsym(const char* str) {
		return symbol_to_value(snow::sym(str));
	}
}



#endif /* end of include guard: SYMBOL_H_RKFL2KE8 */
