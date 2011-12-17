#pragma once
#ifndef ARGUMENTS_H_5M3JAXPP
#define ARGUMENTS_H_5M3JAXPP

#include "snow/value.hpp"
#include "snow/symbol.hpp"

typedef struct SnArguments {
	size_t num_names;
	SnSymbol* names;
	size_t size;
	VALUE* data;
} SnArguments;

#endif /* end of include guard: ARGUMENTS_H_5M3JAXPP */
