#pragma once
#ifndef AST_HPP_DVPCZMTH
#define AST_HPP_DVPCZMTH

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/symbol.hpp"

namespace snow {
	struct LexerLocation {
		uint32_t line;
		uint32_t column;
	};
	
	struct ASTNode;

	struct ASTBase {
		ASTNode* _root;
	};

	void ast_free(ASTBase* ast);
	ASTBase* ast_copy(ASTBase* ast);

	enum ASTNodeType {
		ASTNodeTypeSequence,
		ASTNodeTypeLiteral,
		ASTNodeTypeClosure,
		ASTNodeTypeParameter,
		ASTNodeTypeReturn,
		ASTNodeTypeIdentifier,
		ASTNodeTypeBreak,
		ASTNodeTypeContinue,
		ASTNodeTypeSelf,
		ASTNodeTypeHere,
		ASTNodeTypeIt,
		ASTNodeTypeAssign,
		ASTNodeTypeMethod,
		ASTNodeTypeInstanceVariable,
		ASTNodeTypeCall,
		ASTNodeTypeAssociation,
		ASTNodeTypeNamedArgument,
		ASTNodeTypeAnd,
		ASTNodeTypeOr,
		ASTNodeTypeXor,
		ASTNodeTypeNot,
		ASTNodeTypeLoop,
		ASTNodeTypeIfElse
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
		LexerLocation location;
	};

}

#endif /* end of include guard: AST_HPP_DVPCZMTH */
