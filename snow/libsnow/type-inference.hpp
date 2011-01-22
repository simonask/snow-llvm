#pragma once
#ifndef TYPE_INFERENCE_HPP_EQWFZF1S
#define TYPE_INFERENCE_HPP_EQWFZF1S

#include "snow/value.h"
#include "snow/type.h"
#include "snow/ast.hpp"

namespace snow {
	void infer_expression_types(SnAST* ast);
}

#endif /* end of include guard: TYPE_INFERENCE_HPP_EQWFZF1S */
