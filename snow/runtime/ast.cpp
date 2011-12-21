#include "ast-intern.hpp"

using namespace snow;

static void inprintf(int indent, const char* fmt, ...);

namespace snow {
	void ast_free(ASTBase* _ast) {
		AST* ast = (AST*)_ast;
		delete ast;
	}

	ASTBase* ast_copy(ASTBase* ast) {
		TRAP(); // NIY
		return NULL;
	}
	
	void AST::free(ASTNode* node, bool recursive) {
		if (node && recursive) {
			switch (node->type) {
				case ASTNodeTypeSequence: {
					ASTNode* x = node->sequence.head;
					while (x) {
						ASTNode* next = x->next;
						this->free(x);
						x = next;
					}
					break;
				}
				case ASTNodeTypeClosure: {
					this->free(node->closure.parameters);
					this->free(node->closure.body);
					break;
				}
				case ASTNodeTypeParameter: {
					this->free(node->parameter.id_type);
					this->free(node->parameter.default_value);
					break;
				}
				case ASTNodeTypeReturn: {
					this->free(node->return_expr.value);
					break;
				}
				case ASTNodeTypeAssign: {
					this->free(node->assign.target);
					this->free(node->assign.value);
					break;
				}
				case ASTNodeTypeMethod: {
					this->free(node->method.object);
					break;
				}
				case ASTNodeTypeInstanceVariable: {
					this->free(node->instance_variable.object);
					break;
				}
				case ASTNodeTypeCall: {
					this->free(node->call.object);
					this->free(node->call.args);
					break;
				}
				case ASTNodeTypeAssociation: {
					this->free(node->association.object);
					this->free(node->association.args);
					break;
				}
				case ASTNodeTypeNamedArgument: {
					this->free(node->named_argument.expr);
					break;
				}
				case ASTNodeTypeAnd: 
				case ASTNodeTypeOr:
				case ASTNodeTypeXor: {
					this->free(node->logic_and.left);
					this->free(node->logic_and.right);
					break;
				}
				case ASTNodeTypeNot: {
					this->free(node->logic_not.expr);
					break;
				}
				case ASTNodeTypeLoop: {
					this->free(node->loop.cond);
					this->free(node->loop.body);
					break;
				}
				case ASTNodeTypeIfElse: {
					this->free(node->if_else.cond);
					this->free(node->if_else.body);
					this->free(node->if_else.else_body);
					break;
				}
				default: break;
			}
		}
		_heap.free(node);
	}
	
	void AST::print(ASTNode* n, int indent) const {
		print_r(n ? n : _root, indent);
	}
	
	void AST::print_r(ASTNode* n, int indent) const {
		if (!n) inprintf(indent, "<NULL>\n");
		
		switch (n->type) {
			case ASTNodeTypeSequence: {
				inprintf(indent, "SEQUENCE: ");
				ASTNode* x = n->sequence.head;
				if (x) printf("\n");
				else printf("<EMPTY>\n");
				while (x) {
					print_r(x, indent+1);
					x = x->next;
				}
				break;
			}
			case ASTNodeTypeLiteral: {
				inprintf(indent, "LITERAL: %p\n", n->literal.value);
				break;
			}
			case ASTNodeTypeClosure: {
				inprintf(indent, "CLOSURE: (");
				ASTNode* params = n->closure.parameters;
				if (params) {
					for (ASTNode* x = params->sequence.head; x; x = x->next) {
						ASSERT(x->type == ASTNodeTypeParameter);
						printf("%s%s", snow::sym_to_cstr(x->parameter.name), x->next == NULL ? "" : ", ");
					}
				}
				printf(")\n");
				print_r(n->closure.body, indent+1);
				break;
			}
			case ASTNodeTypeParameter: {
				ASSERT(false); // Should never be reached.
				break;
			}
			case ASTNodeTypeReturn: {
				inprintf(indent, "RETURN");
				if (n->return_expr.value == NULL) printf("\n");
				else {
					printf(":\n");
					print_r(n->return_expr.value, indent+1);
				}
				break;
			}
			case ASTNodeTypeIdentifier: { inprintf(indent, "IDENTIFIER: %s\n", snow::sym_to_cstr(n->identifier.name)); break; }
			case ASTNodeTypeBreak: { inprintf(indent, "BREAK\n"); break; }
			case ASTNodeTypeContinue: { inprintf(indent, "CONTINUE\n"); break; }
			case ASTNodeTypeSelf: { inprintf(indent, "SELF\n"); break; }
			case ASTNodeTypeHere: { inprintf(indent, "HERE\n"); break; }
			case ASTNodeTypeIt: { inprintf(indent, "IT\n"); break; }
			case ASTNodeTypeAssign: {
				inprintf(indent, "ASSIGN:\n");
				inprintf(indent+1, "TARGET:\n");
				print_r(n->assign.target, indent+2);
				inprintf(indent+1, "VALUE:\n");
				print_r(n->assign.value, indent+2);
				break;
			}
			case ASTNodeTypeMethod: {
				inprintf(indent, "METHOD:\n");
				inprintf(indent+1, "NAME: %s\n", snow::sym_to_cstr(n->method.name));
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->method.object, indent+2);
				break;
			}
			case ASTNodeTypeInstanceVariable: {
				inprintf(indent, "INSTANCE VARIABLE:\n");
				inprintf(indent+1, "NAME: %s\n", snow::sym_to_cstr(n->instance_variable.name));
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->instance_variable.object, indent+2);
				break;
			}
			case ASTNodeTypeCall: {
				inprintf(indent, "CALL:\n");
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->call.object, indent+2);
				inprintf(indent+1, "ARGUMENTS:");
				if (n->call.args) {
					printf(" \n");
					int i = 0;
					for (ASTNode* x = n->call.args->sequence.head; x; x = x->next) {
						inprintf(indent+2, "ARG %d:\n", i++);
						print_r(x, indent+3);
					}
				} else {
					printf(" <EMPTY>\n");
				}
				break;
			}
			case ASTNodeTypeAssociation: {
				inprintf(indent, "ASSOCIATION:\n");
				inprintf(indent+1, "OBJECT:\n");
				print_r(n->association.object, indent+2);
				inprintf(indent+1, "ARGUMENTS:\n");
				if (n->association.args) {
					int i = 0;
					for (ASTNode* x = n->association.args->sequence.head; x; x = x->next) {
						inprintf(indent+2, "ARG %d:\n", i++);
						print_r(x, indent+3);
					}
				}
				break;
			}
			case ASTNodeTypeNamedArgument: {
				inprintf(indent, "NAMED ARGUMENT:\n");
				inprintf(indent+1, "NAME: %s\n", snow::sym_to_cstr(n->named_argument.name));
				inprintf(indent+1, "VALUE:\n");
				print_r(n->named_argument.expr, indent+2);
				break;
			}
			case ASTNodeTypeAnd:
			case ASTNodeTypeOr:
			case ASTNodeTypeXor:
			{
				const char* operation = n->type == ASTNodeTypeAnd ? "AND" : (n->type == ASTNodeTypeOr ? "OR" : "XOR");
				inprintf(indent, "LOGIC %s:\n", operation);
				inprintf(indent+1, "LEFT:\n");
				print_r(n->logic_and.left, indent+2);
				inprintf(indent+2, "RIGHT:\n");
				print_r(n->logic_and.right, indent+2);
				break;
			}
			case ASTNodeTypeNot: {
				inprintf(indent, "NOT:\n");
				print_r(n->logic_not.expr, indent+1);
				break;
			}
			case ASTNodeTypeLoop: {
				inprintf(indent, "LOOP:\n");
				inprintf(indent+1, "CONDITION:\n");
				print_r(n->loop.cond, indent+2);
				inprintf(indent+1, "BODY:\n");
				print_r(n->loop.body, indent+2);
				break;
			}
			case ASTNodeTypeIfElse: {
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
