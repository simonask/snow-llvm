#include "snow/ast.hpp"



using namespace snow;


static void inprintf(int indent, const char* fmt, ...);


CAPI void snow_ast_free(SnAST* _ast) {
	AST* ast = (AST*)_ast;
	delete ast;
}

CAPI SnAST* snow_ast_copy(SnAST* ast) {
	TRAP(); // NIY
}


namespace snow {
	void AST::free(SnAstNode* node, bool recursive) {
		
	}
	
	void AST::print(SnAstNode* n, int indent) const {
		print_r(n ? n : _root, indent);
	}
	
	void AST::print_r(SnAstNode* n, int indent) const {
		if (!n) inprintf(indent, "<NULL>\n");
		
		switch (n->type) {
			case SN_AST_SEQUENCE: {
				inprintf(indent, "SEQUENCE: ");
				SnAstNode* x = n->sequence.head;
				if (x) printf("\n");
				else printf("<EMPTY>\n");
				while (x) {
					print_r(x, indent+1);
					x = x->next;
				}
				break;
			}
			case SN_AST_LITERAL: {
				inprintf(indent, "LITERAL: %p\n", n->literal.value);
				break;
			}
			case SN_AST_CLOSURE: {
				inprintf(indent, "CLOSURE: (");
				SnAstNode* params = n->closure.parameters;
				if (params) {
					for (SnAstNode* x = params->sequence.head; x; x = x->next) {
						ASSERT(x->type == SN_AST_PARAMETER);
						printf("%s%s", snow_sym_to_cstr(NULL, x->parameter.name), x->next == NULL ? "" : ", ");
					}
				}
				printf(")\n");
				print_r(n->closure.body, indent+1);
				break;
			}
			case SN_AST_PARAMETER: {
				ASSERT(false); // Should never be reached.
				break;
			}
			case SN_AST_RETURN: {
				inprintf(indent, "RETURN");
				if (n->return_expr.value == NULL) printf("\n");
				else {
					printf(":\n");
					print_r(n->return_expr.value, indent+1);
				}
				break;
			}
			case SN_AST_IDENTIFIER: { inprintf(indent, "IDENTIFIER: %s\n", snow_sym_to_cstr(NULL, n->identifier.name)); break; }
			case SN_AST_BREAK: { inprintf(indent, "BREAK\n"); break; }
			case SN_AST_CONTINUE: { inprintf(indent, "CONTINUE\n"); break; }
			case SN_AST_SELF: { inprintf(indent, "SELF\n"); break; }
			case SN_AST_HERE: { inprintf(indent, "HERE\n"); break; }
			case SN_AST_IT: { inprintf(indent, "IT\n"); break; }
			case SN_AST_ASSIGN: {
				inprintf(indent, "ASSIGN:\n");
				inprintf(indent+1, "TARGET:\n");
				print_r(n->assign.target, indent+2);
				inprintf(indent+1, "VALUE:\n");
				print_r(n->assign.value, indent+2);
				break;
			}
			case SN_AST_MEMBER: {
				inprintf(indent, "MEMBER:\n");
				inprintf(indent+1, "NAME: %s\n", snow_sym_to_cstr(NULL, n->member.name));
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->member.object, indent+2);
				break;
			}
			case SN_AST_CALL: {
				inprintf(indent, "CALL:\n");
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->call.object, indent+2);
				inprintf(indent+1, "ARGUMENTS:");
				if (n->call.args) {
					printf(" \n");
					int i = 0;
					for (SnAstNode* x = n->call.args->sequence.head; x; x = x->next) {
						inprintf(indent+2, "ARG %d:\n", i++);
						print_r(x, indent+3);
					}
				} else {
					printf(" <EMPTY>\n");
				}
				break;
			}
			case SN_AST_ASSOCIATION: {
				inprintf(indent, "ASSOCIATION:\n");
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->association.object, indent+2);
				inprintf(indent+1, "ARGUMENTS:\n");
				if (n->association.args) {
					int i = 0;
					for (SnAstNode* x = n->association.args->sequence.head; x; x = x->next) {
						inprintf(indent+2, "ARG %d:\n", i++);
						print_r(x, indent+3);
					}
				}
				break;
			}
			case SN_AST_NAMED_ARGUMENT: {
				inprintf(indent, "NAMED ARGUMENT:\n");
				inprintf(indent+1, "NAME: %s\n", snow_sym_to_cstr(NULL, n->named_argument.name));
				inprintf(indent+1, "VALUE:\n");
				print_r(n->named_argument.expr, indent+2);
				break;
			}
			case SN_AST_AND:
			case SN_AST_OR:
			case SN_AST_XOR:
			{
				const char* operation = n->type == SN_AST_AND ? "AND" : (n->type == SN_AST_OR ? "OR" : "XOR");
				inprintf(indent, "LOGIC %s:\n", operation);
				inprintf(indent+1, "LEFT:\n");
				print_r(n->logic_and.left, indent+2);
				inprintf(indent+2, "RIGHT:\n");
				print_r(n->logic_and.right, indent+2);
				break;
			}
			case SN_AST_NOT: {
				inprintf(indent, "NOT:\n");
				print_r(n->logic_not.expr, indent+1);
				break;
			}
			case SN_AST_LOOP: {
				inprintf(indent, "LOOP:\n");
				inprintf(indent+1, "CONDITION:\n");
				print_r(n->loop.cond, indent+2);
				inprintf(indent+1, "BODY:\n");
				print_r(n->loop.body, indent+2);
				break;
			}
			case SN_AST_IF_ELSE: {
				inprintf(indent, "IF:\n");
				inprintf(indent+1, "CONDITION:\n");
				print_r(n->if_else.cond, indent+2);
				inprintf(indent+1, "BODY:\n");
				print_r(n->if_else.body, indent+2);
				if (n->if_else.else_body) {
					inprintf(indent+1, "ELSE:\n");
					print_r(n->if_else.else_body, indent+2);
				}
				break;
			}
		}
	}
}


static void inprintf(int indent, const char* fmt, ...) {
	for (int i = 0; i < indent; ++i) printf(" ");
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
