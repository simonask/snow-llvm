#pragma once
#ifndef PARSER_H_OX0F2RR9
#define PARSER_H_OX0F2RR9

#include "snow/basic.h"
#include "snow/ast.hpp"

namespace snow {
	ASTBase* parse(const char* buffer);
}

#endif /* end of include guard: PARSER_H_OX0F2RR9 */
