#pragma once
#ifndef PARSER_H_OX0F2RR9
#define PARSER_H_OX0F2RR9

#include "snow/basic.h"
#include "snow/ast.hpp"
#include <string>

namespace snow {
	ASTBase* parse(const std::string& path, const std::string& source);
}

#endif /* end of include guard: PARSER_H_OX0F2RR9 */
