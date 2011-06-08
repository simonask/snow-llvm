#pragma once
#ifndef SYMBOL_HPP_E0OCZIQR
#define SYMBOL_HPP_E0OCZIQR

#include "snow/symbol.h"

namespace snow {
	struct Symbol {
		Symbol() : sym(0) {}
		Symbol(const char* name) : sym(snow_sym(name)) {}
		Symbol(SnSymbol sym) : sym(sym) {}
		bool operator==(const Symbol& other) const { return sym == other.sym; }
		bool operator!=(const Symbol& other) const { return sym != other.sym; }
		Symbol& operator(const Symbol& other) { sym = other.sym; return *this; }
		bool operator<(const Symbol& other) { return sym < other.sym; }
		const char* c_str() const { return snow_sym_to_cstr(sym); }
		
		SnSymbol sym;
	};
}

#endif /* end of include guard: SYMBOL_HPP_E0OCZIQR */
