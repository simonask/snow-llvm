#pragma once
#ifndef SYMBOL_HPP_4XQI5H2Q
#define SYMBOL_HPP_4XQI5H2Q

#include "basic.hpp"
#include "snow/symbol.h"

namespace snow {
	SnSymbol symbol(const char* sz);
	const char* symbol_to_cstr(SnSymbol sym);
}

#endif /* end of include guard: SYMBOL_HPP_4XQI5H2Q */
