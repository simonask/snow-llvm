#pragma once
#ifndef SYMBOL_H_RKFL2KE8
#define SYMBOL_H_RKFL2KE8

#include "snow/basic.h"
#include "snow/value.hpp"

namespace snow {
	typedef uint64_t Symbol;

	Symbol sym(const char* str);
	const char* sym_to_cstr(Symbol sym);

	INLINE bool is_symbol(const Immediate& val) {
		return ((uintptr_t)val.value() & ValueTypeMask) == SymbolType;
	}

	INLINE Immediate symbol_to_value(Symbol sym) {
		return Immediate((VALUE)(sym << 4 | SymbolType));
	}

	INLINE Symbol value_to_symbol(const Immediate& val) {
		ASSERT(is_symbol(val));
		return (Symbol)((uintptr_t)val.value() >> 4);
	}

	INLINE Immediate vsym(const char* str) {
		return symbol_to_value(snow::sym(str));
	}
}



#endif /* end of include guard: SYMBOL_H_RKFL2KE8 */
