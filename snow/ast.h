#pragma once
#ifndef AST_H_48SOBLU9
#define AST_H_48SOBLU9

#include "snow/basic.h"

struct SnAST;

CAPI void snow_ast_free(struct SnAST* ast);
CAPI struct SnAST* snow_ast_copy(struct SnAST* ast);

#endif /* end of include guard: AST_H_48SOBLU9 */
