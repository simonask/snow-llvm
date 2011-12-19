#pragma once
#ifndef AST_HPP_DVPCZMTH
#define AST_HPP_DVPCZMTH

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/symbol.hpp"

namespace snow {
	struct ASTNode;

	struct ASTBase {
		ASTNode* _root;
	};

	void ast_free(ASTBase* ast);
	ASTBase* ast_copy(ASTBase* ast);

	enum ASTNodeType {
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
		SN_AST_METHOD,
		SN_AST_INSTANCE_VARIABLE,
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

	struct ASTNode {
		ASTNodeType type;
		ASTNode* next; // implicit linked list for sequences -- NULL otherwise
		union {
			struct { ASTNode *head, *tail; size_t length; } sequence;
			struct { VALUE value; }                         literal;
			struct { ASTNode *parameters, *body; }          closure;
			struct { Symbol name; ASTNode *id_type, *default_value; } parameter;
			struct { ASTNode *value; }                   return_expr;
			struct { Symbol name; }                      identifier;
			struct { ASTNode *target, *value; }          assign;
			struct { ASTNode *object; Symbol name; }     method;
			struct { ASTNode *object; Symbol name; }     instance_variable;
			struct { ASTNode *object, *args; }           call;
			struct { ASTNode *object, *args; }           association;
			struct { Symbol name; ASTNode *expr; }       named_argument;
			struct { ASTNode *left, *right; }            logic_and, logic_or, logic_xor;
			struct { ASTNode *expr; }                    logic_not;
			struct { ASTNode *cond, *body; }             loop;
			struct { ASTNode *cond, *body, *else_body; } if_else;
		};
	};

}

#endif /* end of include guard: AST_HPP_DVPCZMTH */
