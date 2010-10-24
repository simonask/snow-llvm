#pragma once
#ifndef AST_HPP_DVPCZMTH
#define AST_HPP_DVPCZMTH

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"
#include "snow/ast.h"

#include "snow/linkheap.hpp"

struct SnAST {};

enum SnSnAstNodeType {
	SN_AST_SEQUENCE,
	SN_AST_LITERAL,
	SN_AST_CLOSURE,
	SN_AST_PARAMETER,
	SN_AST_RETURN,
	SN_AST_IDENTIFIER,
	SN_AST_BREAK,
	SN_AST_CONTINUE,
	SN_AST_SELF,
	SN_AST_HERE,
	SN_AST_IT,
	SN_AST_ASSIGN,
	SN_AST_MEMBER,
	SN_AST_CALL,
	SN_AST_ASSOCIATION,
	SN_AST_NAMED_ARGUMENT,
	SN_AST_AND,
	SN_AST_OR,
	SN_AST_XOR,
	SN_AST_NOT,
	SN_AST_LOOP,
	SN_AST_IF_ELSE
};

struct SnAstNode {
	SnSnAstNodeType type;
	SnAstNode* next; // implicit linked list for sequences -- NULL otherwise
	union {
		struct { SnAstNode *head, *tail; }             sequence;
		struct { VALUE value; }                        literal;
		struct { SnAstNode *parameters, *body; }       closure;
		struct { SnSymbol name; SnAstNode *id_type, *default_value; } parameter;
		struct { SnAstNode *value; }                   return_expr;
		struct { SnSymbol name; }                      identifier;
		struct { SnAstNode *target, *value; }          assign;
		struct { SnAstNode *object; SnSymbol name; }   member;
		struct { SnAstNode *object, *args; }           call;
		struct { SnAstNode *object, *args; }           association;
		struct { SnSymbol name; SnAstNode *expr; }     named_argument;
		struct { SnAstNode *left, *right; }            logic_and, logic_or, logic_xor;
		struct { SnAstNode *expr; }                    logic_not;
		struct { SnAstNode *cond, *body; }             loop;
		struct { SnAstNode *cond, *body, *else_body; } if_else;
	};
};

#endif /* end of include guard: AST_HPP_DVPCZMTH */
