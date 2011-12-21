#include "snow/parser.hpp"
#include "snow/symbol.hpp"
#include "snow/numeric.hpp"
#include "snow/str.hpp"

#include "linkheap.hpp"
#include "ast-intern.hpp"
#include "lexer.hpp"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

#include <vector>
#include <utility>

#define GET_TOKEN_SZ(SZ, TOKEN) char* SZ = (char*)alloca((TOKEN)->length+1); memcpy(SZ, (TOKEN)->begin, (TOKEN)->length); (SZ)[(TOKEN)->length] = '\0'

#define FIRST_MATCH(BLOCK) do BLOCK while(0)
#define MATCH(RULE) { Pos p = pos; ASTNode* r = RULE(p); if (r) { result = r; pos = p; break; } }

#define PARSER_DEBUG 0

#if PARSER_DEBUG
#define MATCH_FAILED() { fprintf(stderr, "DEBUG: FAIL '%s' (line %d, %s).\n", __func__, __LINE__, position_to_cstr(pos)); return NULL; }
#define MATCH_SUCCESS(X) { fprintf(stderr, "DEBUG: MATCH '%s' (line %d, %s).\n", __func__, __LINE__, position_to_cstr(pos)); return X; }
#else
#define MATCH_FAILED() { return NULL; }
#define MATCH_SUCCESS(X) { return X; }
#endif

namespace snow {
	class Parser {
	public:
		typedef Lexer::iterator Pos;
		
		Parser(Pos pos) : _pos(pos), _error_message(NULL) {}
		~Parser() { free(_error_message); }
		
		AST* parse();
		
	private:
		void error(Pos error_pos, const char* message, ...);
		
		ASTNode* sequence(Pos&);
		ASTNode* statement(Pos&);
		ASTNode* expression(Pos&);
		ASTNode* expression_no_assign(Pos&);
		ASTNode* control(Pos&);
		ASTNode* operation(Pos&);
		ASTNode* operand(Pos&);
		ASTNode* operand_with_unary(Pos&);
		ASTNode* assignment(Pos&);
		ASTNode* assign_target(Pos&);
		ASTNode* assign_values(Pos&);
		ASTNode* lvalue(Pos&);
		ASTNode* lvalues(Pos&);
		ASTNode* postcondition(Pos&);
		ASTNode* postloop(Pos&);
		ASTNode* condition(Pos&);
		ASTNode* loop(Pos&);
		ASTNode* atomic_expression(Pos&);
		ASTNode* chain(Pos&);
		ASTNode* chainable(Pos&);
		ASTNode* call(Pos&);
		ASTNode* method(Pos&);
		ASTNode* instance_variable(Pos&);
		ASTNode* argument_list(Pos&);
		ASTNode* argument(Pos&);
		ASTNode* named_argument(Pos&);
		ASTNode* association_key_list(Pos&);
		ASTNode* closure(Pos&);
		ASTNode* parameter_list(Pos&);
		ASTNode* parameter(Pos&);
		ASTNode* identifier(Pos&);
		ASTNode* literal(Pos&);
		ASTNode* symbol(Pos&);
		ASTNode* self(Pos&);
		ASTNode* it(Pos&);
		ASTNode* here(Pos&);
		
		ASTNode* parse_post_terms(Pos&, ASTNode* for_expr); // postconditions and postloops
		bool skip_end_of_statement(Pos&);
		
		ASTNode* reduce_operation(ASTNode* a, ASTNode* b, Pos op);
	private:
		class PrecedenceParser;
		friend class PrecedenceParser;
		
		Pos _pos;
		AST* ast;
		jmp_buf _error_handler;
		char* _error_message;
		Pos _error_pos;
	};
	
	class Parser::PrecedenceParser {
	public:
		typedef std::pair<Token::Type, Parser::Pos> Op;
		Parser* parser;
		std::vector<Op> operators;
		std::vector<ASTNode*> operands;
		
		PrecedenceParser(Parser* parser, ASTNode* first) : parser(parser) {
			operands.push_back(first);
			operators.push_back(Op(Token::INVALID, Parser::Pos()));
		}
		
		void push_operand(ASTNode* a) {
			operands.push_back(a);
		}
		
		void push_operator(const Parser::Pos& pos) {
			while (operators.back().first >= pos->type) { // '>=' means left-associative -- > would mean right-associative
				pop_operator();
			}
			operators.push_back(Op(pos->type, pos));
		}
		void pop_operator() {
			ASTNode* b = operands.back();
			operands.pop_back();
			ASTNode* a = operands.back();
			operands.pop_back();
			Parser::Pos op = operators.back().second;
			operators.pop_back();
			
			ASTNode* reduced = parser->reduce_operation(a, b, op);
			operands.push_back(reduced);
		}
		
		ASTNode* reduce() {
			while (operands.size() > 1) { pop_operator(); }
			return operands.back();
		}
	};
	
	static inline bool is_operator(Token::Type t) {
		switch (t) {
			case Token::OPERATOR_FIRST:
			case Token::OPERATOR_SECOND:
			case Token::OPERATOR_THIRD:
			case Token::OPERATOR_FOURTH:
			case Token::LOG_AND:
			case Token::LOG_OR:
			case Token::LOG_XOR:
				return true;
			default: return false;
		}
	}
	
	
	static inline const char* position_to_cstr(const Parser::Pos& pos) {
		static char* data = NULL;
		free(data);
		int lineno = pos->line_number;
		int column = (int)(pos->begin - pos->line_begin);
		asprintf(&data, "stdin:%d:%d:%s", lineno, column, get_token_name(pos->type));
		return data;
	}
	
	
	AST* Parser::parse() {
		ast = new AST;
		ast->set_root(NULL);
		if (!setjmp(_error_handler)) {
			ASTNode* seq = sequence(_pos);
			if (_pos->type != Token::END_OF_FILE) {
				error(_pos, "Expected END_OF_FILE, got '%s'!", get_token_name(_pos->type));
			}
			ast->set_root(seq);
			return ast;
		} else {
			long col = _error_pos->begin - _error_pos->line_begin;
			
			fprintf(stderr, "PARSER ERROR at line %d col %ld: %s\n", _error_pos->line_number, col, _error_message);
			
			// print context:
			const char* p = _error_pos->line_begin;
			while (*p && *p != '\n') {
				if (*p == '\t') {
					col += 3;
					printf("    ");
					++p;
				} else {
					putchar(*p++);
				}
				
			}
			putchar('\n');
			for (long i = 0; i < col; ++i) putchar('~');
			putchar('^');
			putchar('\n');
			
			delete ast;
			return NULL;
		}
	}
	
	void Parser::error(Pos error_pos, const char* message, ...) {
		_error_pos = error_pos;
		va_list ap;
		va_start(ap, message);
		vasprintf(&_error_message, message, ap);
		va_end(ap);
		longjmp(_error_handler, 1);
	}
	
	ASTNode* Parser::sequence(Pos& pos) {
		/*
			sequence := separator* statement*;
		*/
		while (skip_end_of_statement(pos));
		ASTNode* seq = ast->sequence();
		while (!pos.at_end()) {
			ASTNode* s = statement(pos);
			if (s) ast->sequence_push(seq, s);
			else break;
		}
		MATCH_SUCCESS(seq);
	}
	
	ASTNode* Parser::statement(Pos& pos) {
		/*
			statement := (expression | control) separator*;
		*/
		ASTNode* result = NULL;
		FIRST_MATCH({
			MATCH(expression);
			MATCH(control);
		});
		if (result) { while (skip_end_of_statement(pos)); }
		MATCH_SUCCESS(result);
	}
	
	ASTNode* Parser::expression(Pos& pos) {
		/*
			expression := (assignment | operation) (postcondition | postloop)*;
		*/
		ASTNode* result = NULL;
		FIRST_MATCH({
			MATCH(assignment);
			MATCH(operation);
		});
		if (!result) { MATCH_FAILED(); }
		
		result = parse_post_terms(pos, result);
		MATCH_SUCCESS(result);
	}
	
	ASTNode* Parser::expression_no_assign(Pos& pos) {
		/*
			expression_no_assign := operation (postcondition | postloop)*;
		*/
		ASTNode* expr = operation(pos);
		if (expr) {
			expr = parse_post_terms(pos, expr);
			MATCH_SUCCESS(expr);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::control(Pos& pos) {
		/*
			control := (return | BREAK | CONTINUE) postcondition?;
			return := RETURN operand;
		*/
		ASTNode* result = NULL;
		if (pos->type == Token::BREAK) { result = ast->break_(); ++pos; }
		else if (pos->type == Token::CONTINUE) { result = ast->continue_(); ++pos; }
		else if (pos->type == Token::RETURN) {
			++pos;
			ASTNode* v = operand(pos);
			result = ast->return_(v);
		}
		
		if (result) {
			ASTNode* postcond = postcondition(pos);
			if (postcond) {
				postcond->if_else.body = result;
				result = postcond;
			}
			MATCH_SUCCESS(result);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::operation(Pos& pos) {
		/*
			operation := operand (operator operand)*;
		*/
		Pos p = pos;
		ASTNode* a = operand_with_unary(p);
		
		if (a) {
			PrecedenceParser prec(this, a);
			while (is_operator(p->type)) {
				prec.push_operator(p);
				++p;
				a = operand_with_unary(p);
				if (!a) {
					// XXX: ERROR!! LEAKING!!!
					error(p, "Expected second operand, got %s.", get_token_name(p->type));
					MATCH_FAILED();
				}
				prec.push_operand(a);
			}
			a = prec.reduce();
		}
		
		if (a) { pos = p; MATCH_SUCCESS(a); }
		MATCH_FAILED();
	}
	
	ASTNode* Parser::operand(Pos& pos) {
		/*
			operand :=  operator? (condition | loop | chain | atomic_expression);
		*/
		
		ASTNode* result = NULL;
		FIRST_MATCH({
			MATCH(condition);
			MATCH(loop);
			MATCH(chain);
			MATCH(atomic_expression);
		});
		if (result)
			MATCH_SUCCESS(result);
		MATCH_FAILED();
	}
	
	ASTNode* Parser::operand_with_unary(Pos& pos) {
		Pos p = pos;
		Token::Type type = p->type;
		if (is_operator(type) || type == Token::LOG_NOT) {
			GET_TOKEN_SZ(data, p);
			++p;
			ASTNode* a = operand(p);
			if (!a) {
				error(p, "Expected operand for unary operator, got %s.", get_token_name(p->type));
				MATCH_FAILED();
			}
			pos = p;
			if (type == Token::LOG_NOT)
				return ast->logic_not(a);
			return ast->call(ast->method(a, snow::sym(data)), NULL);
		}
		return operand(pos);
	}
	
	ASTNode* Parser::assign_target(Pos& pos) {
		Pos p = pos;
		ASTNode* lvs = lvalues(p);
		if (lvs && p->type == Token::ASSIGN) {
			pos = ++p;
			MATCH_SUCCESS(ast->assign(lvs, NULL));
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::assign_values(Pos& pos) {
		Pos p = pos;
		ASTNode* a = operation(p);
		if (a) {
			ASTNode* seq = ast->sequence(1, a);
			while (p->type == Token::COMMA) {
				++p;
				a = operation(p);
				if (a) ast->sequence_push(seq, a);
				else break;
			}
			pos = p;
			MATCH_SUCCESS(seq);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::assignment(Pos& pos) {
		/*
			assignment := (lvalues ':')+ operation (',' operation)*;
		*/
		Pos p = pos;
		ASTNode* result = assign_target(p);
		if (result) {
			ASTNode* last_assign = result;
			ASTNode* assign = assign_target(p);
			while (assign) {
				last_assign->assign.value = assign;
				last_assign = assign;
				assign = assign_target(p);
			}
			
			ASTNode* values = assign_values(p);
			if (values) {
				last_assign->assign.value = values;
			} else {
				error(p, "Unexpected token %s.", get_token_name(p->type));
			}
		}
		if (result) { pos = p; MATCH_SUCCESS(result); }
		MATCH_FAILED();
	}
	
	ASTNode* Parser::lvalue(Pos& pos) {
		/*
			lvalue := association | instance_variable | identifier | method;
			
			TODO:
			lvalue := chain_that_doesnt_end_with_call | identifier;
		*/
		ASTNode* result = chain(pos);
		if (result) MATCH_SUCCESS(result);
		MATCH_FAILED();
	}
	
	ASTNode* Parser::lvalues(Pos& pos) {
		/*
			lvalues := lvalue (',' lvalue)*;
		*/
		
		ASTNode* lv = lvalue(pos);
		if (lv) {
			ASTNode* seq = ast->sequence(1, lv);
			while (pos->type == Token::COMMA) {
				++pos;
				lv = lvalue(pos);
				if (lv) ast->sequence_push(seq, lv);
				else {
					error(pos, "Invalid lvalue %s.", get_token_name(pos->type));
					MATCH_FAILED();
				}
			}
			MATCH_SUCCESS(seq);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::postcondition(Pos& pos) {
		Token::Type type = pos->type;
		if (type == Token::IF || type == Token::UNLESS) {
			++pos;
			ASTNode* op = operation(pos);
			if (!op) {
				error(pos, "Expected expression for postcondition, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			if (type == Token::UNLESS) {
				op = ast->logic_not(op);
			}
			ASTNode* cond = ast->if_else(op, NULL, NULL);
			MATCH_SUCCESS(cond);
		}
		MATCH_FAILED();
	}
	ASTNode* Parser::postloop(Pos& pos) {
		/*
			postloop := (WHILE | UNTIL) operation;
		*/
		Token::Type type = pos->type;
		if (type == Token::WHILE || type == Token::UNTIL) {
			++pos;
			ASTNode* op = operation(pos);
			if (!op) {
				error(pos, "Expected expression for postloop, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			if (type == Token::UNTIL) {
				op = ast->logic_not(op);
			}
			MATCH_SUCCESS(ast->loop(op, NULL));
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::condition(Pos& pos) {
		/*
			basic_condition := (IF | UNLESS) operation (THEN | separator) sequence (ELSE separator sequence)? END
			condition := (IF | UNLESS)
		*/
		
		ASTNode* toplevel_condition = NULL;
		ASTNode** place_last_here = &toplevel_condition;
		
		while (pos->type == Token::IF || pos->type == Token::UNLESS) {
			Token::Type type = pos->type;
			ASTNode* condition = operation(++pos);
			if (!condition) {
				error(pos, "Expected expression for conditional, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			if (type == Token::UNLESS) { condition = ast->logic_not(condition); }
			
			if (pos->type == Token::THEN || skip_end_of_statement(pos)) {
				while (skip_end_of_statement(pos)); // skip additional ends
				ASTNode* body = sequence(pos);
				
				if (pos->type == Token::END) {
					*place_last_here = ast->if_else(condition, body, NULL);
					MATCH_SUCCESS(toplevel_condition);
				} else if (pos->type == Token::ELSE) {
					if ((pos+1)->type == Token::IF) {
						++pos;
						// else if: let's go again!
						*place_last_here = ast->if_else(condition, body, NULL);
						place_last_here = &(*place_last_here)->if_else.else_body;
					} else {
						ASTNode* else_body = sequence(++pos);
						if (pos->type != Token::END) {
							error(pos, "Expected END of conditional, got %s.", get_token_name(pos->type));
							MATCH_FAILED();
						}
						++pos;
						
						*place_last_here = ast->if_else(condition, body, else_body);
						MATCH_SUCCESS(toplevel_condition);
					}
				}

			} else {
				error(pos, "Expected THEN or end of statement, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
		}
		
		MATCH_FAILED();
	}
	
	ASTNode* Parser::loop(Pos& pos) {
		/*
			loop := (WHILE | UNTIL) operation (DO | separator) sequence END;
		*/
		Token::Type type = pos->type;
		if (type == Token::WHILE || type == Token::UNTIL) {
			ASTNode* op = operation(++pos);
			if (!op) {
				error(pos, "Expected expression for loop condition, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			if (type == Token::UNTIL) { op = ast->logic_not(op); }
			
			if (pos->type == Token::DO || skip_end_of_statement(pos)) {
				while (skip_end_of_statement(pos)); // skip additional ends
				ASTNode* body = sequence(pos);
				if (pos->type == Token::END) {
					++pos;
					ASTNode* loop = ast->loop(op, body);
					MATCH_SUCCESS(loop);
				} else {
					error(pos, "Expected END, got %s.", get_token_name(pos->type));
					MATCH_FAILED();
				}
			} else {
				error(pos, "Expected DO or end of statement, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::atomic_expression(Pos& pos) {
		/*
			atomic_expression := literal | instance_variable | self | it | here | identifier | closure | '(' expression ')';
		*/
		
		ASTNode* result = NULL;
		FIRST_MATCH({
			MATCH(literal);
			MATCH(instance_variable);
			MATCH(self);
			MATCH(it);
			MATCH(here);
			MATCH(identifier);
			MATCH(closure);
		});
		if (!result) {
			if (pos->type == Token::PARENS_BEGIN) {
				++pos;
				result = expression(pos);
				if (!result) {
					error(pos, "Expected expression between parantheses, got %s.", get_token_name(pos->type));
					MATCH_FAILED();
				}
				if (pos->type == Token::PARENS_END) {
					++pos;
					MATCH_SUCCESS(result);
				} else {
					error(pos, "Expected ')', got %s.", get_token_name(pos->type));
					MATCH_FAILED();
				}
			}
		}
		MATCH_SUCCESS(result);
	}
	
	ASTNode* Parser::instance_variable(Pos& pos){
		if (pos->type == Token::DOLLAR) {
			++pos;
			ASTNode* id = identifier(pos);
			if (!id) {
				error(pos, "Expected identifier for name of instance variable, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			Symbol sym = id->identifier.name;
			ast->free(id);
			MATCH_SUCCESS(ast->instance_variable(ast->self(), sym));
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::chain(Pos& pos) {
		/*
			chain := (call | instance_variable | method | association)+;
		*/
		Pos p = pos;
		ASTNode* result = atomic_expression(p);
		if (!result && p->type == Token::DOT)
			result = ast->self();
		
		bool keep_going = true;
		while (keep_going) {
			switch (p->type) {
				case Token::DOT: {
					++p;
					if (p->type == Token::DOLLAR) {
						++p;
						ASTNode* id = identifier(p);
						if (!id) {
							error(p, "Expected identifier for name of instance variable, got %s.", get_token_name(p->type));
							MATCH_FAILED();
						}
						Symbol sym = id->identifier.name;
						ast->free(id);
						result = ast->instance_variable(result, sym);
					} else {
						ASTNode* id = identifier(p);
						if (!id) {
							error(p, "Expected identifier as right hand of dot operator, got %s.", get_token_name(p->type));
							MATCH_FAILED();
						}
						Symbol sym = id->identifier.name;
						ast->free(id);
						result = ast->method(result, sym);
					}
					break;
				}
				case Token::BRACE_BEGIN:
					// closure call without parameters
				case Token::PARENS_BEGIN: {
					// regular call
					ASTNode* arg_list = argument_list(p);
					if (!arg_list) {
						error(p, "Expected argument list for method call, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					result = ast->call(result, arg_list);
					break;
				}
				case Token::BRACKET_BEGIN: {
					// either association, or closure call with parameters
					ASTNode* block = closure(p);
					if (block) {
						result = ast->call(result, ast->sequence(1, block));
						break;
					}
					ASTNode* assoc_key = association_key_list(p);
					if (!assoc_key) {
						error(p, "Expected closure call or dictionary key, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					result = ast->association(result, assoc_key);
					break;
				}
				default: {
					keep_going = false;
					break;
				}
			}
		}
		if (result) { pos = p; MATCH_SUCCESS(result); }
		MATCH_FAILED();
	}
	
	ASTNode* Parser::argument_list(Pos& pos) {
		/*
			argument_list := closure | ('(' (argument (',' argument)*)? ')' closure?);
		*/
		ASTNode* result = closure(pos);
		if (result) MATCH_SUCCESS(ast->sequence(1, result));
		
		Pos p = pos;
		if (p->type == Token::PARENS_BEGIN) {
			++p;
			while (skip_end_of_statement(p));
			ASTNode* seq = ast->sequence();
			bool first = true;
			while (p->type != Token::PARENS_END) {
				if (!first) {
					if (p->type != Token::COMMA) {
						error(p, "Expected COMMA in function argument list, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					++p;
					while (skip_end_of_statement(p));
					if (p->type == Token::PARENS_END) break; // permit comma at end of list
				}
				first = false;
				
				ASTNode* arg = argument(p);
				if (!arg) {
					error(p, "Invalid function argument, got %s.", get_token_name(p->type));
					MATCH_FAILED();
				}
				ast->sequence_push(seq, arg);
				while (skip_end_of_statement(p));
			}
			ASSERT(p->type == Token::PARENS_END);
			++p; // skip PARENS_END
			
			ASTNode* block = closure(p);
			if (block) ast->sequence_push(seq, block);
			pos = p;
			MATCH_SUCCESS(seq);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::argument(Pos& pos) {
		/*
			argument := named_argument | (operation (postcondition | postloop)*));
		*/
		ASTNode* named_arg = named_argument(pos);
		if (named_arg) MATCH_SUCCESS(named_arg);
		
		ASTNode* expr = operation(pos);
		if (expr) {
			expr = parse_post_terms(pos, expr);
			MATCH_SUCCESS(expr);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::named_argument(Pos& pos) {
		/*
			named_argument := identifier ':' expression_no_assign;
		*/
		Pos p = pos;
			
		ASTNode* id = identifier(p);
		if (!id) MATCH_FAILED();
		if (p->type != Token::ASSIGN) MATCH_FAILED();
		++p;
		ASTNode* expr = expression_no_assign(p);
		if (expr) {
			Symbol name = id->identifier.name;
			ast->free(id);
			pos = p;
			MATCH_SUCCESS(ast->named_argument(name, expr));
		}
		error(p, "Expected right-hand side for named argument, got %s.", get_token_name(p->type));
		MATCH_FAILED();
	}
	
	ASTNode* Parser::association_key_list(Pos& pos) {
		/*
			association := association_target ('[' expression_no_assign (',' expression_no_assign)* ']')+
		*/
		if (pos->type == Token::BRACKET_BEGIN) {
			++pos;
			bool first = true;
			ASTNode* seq = ast->sequence();
			
			while (pos->type != Token::BRACKET_END) {
				if (!first) {
					if (pos->type != Token::COMMA) {
						error(pos, "Expected COMMA or BRACKET_END in association key list, got %s.", get_token_name(pos->type));
						MATCH_FAILED();
					}
					++pos;
				}
				first = false;
				
				ASTNode* expr = expression_no_assign(pos);
				if (expr) {
					ast->sequence_push(seq, expr);
				} else {
					error(pos, "Expected valid expression for association key list, got %s.", get_token_name(pos->type));
					MATCH_FAILED();
				}
			}
			
			ASSERT(pos->type == Token::BRACKET_END);
			++pos; // skip BRACKET_END
			
			MATCH_SUCCESS(seq);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::closure(Pos& pos) {
		/*
			closure := parameter_list? '{' sequence '}';
		*/
		Pos p = pos;
		ASTNode* params = parameter_list(p);
		if (p->type == Token::BRACE_BEGIN) {
			// TODO: Save beginning position to give better error messages.
			++p;
			ASTNode* body = sequence(p);
			if (p->type == Token::BRACE_END) {
				++p;
				pos = p;
				MATCH_SUCCESS(ast->closure(params, body));
			}
			error(p, "Expected BRACE_END, got %s.", get_token_name(p->type));
			MATCH_FAILED();
		} else {
			// probably not a closure after all
			ast->free(params);
			MATCH_FAILED();
		}
	}
	
	ASTNode* Parser::parameter_list(Pos& pos) {
		/*
			parameter_list := '[' (parameter (',' parameter)*)? ']';
		*/
		Pos p = pos;
		if (p->type == Token::BRACKET_BEGIN) {
			++p;
			ASTNode* seq = ast->sequence();
			
			bool first = true;
			while (p->type != Token::BRACKET_END) {
				if (!first) {
					if (p->type != Token::COMMA) {
						error(p, "Expected COMMA in parameter list, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					++p;
				}
				first = false;
				
				ASTNode* param = parameter(p);
				if (param) {
					ast->sequence_push(seq, param);
				} else {
					ast->free(seq);
					MATCH_FAILED();
				}
			}
			
			ASSERT(p->type == Token::BRACKET_END);
			pos = ++p;
			MATCH_SUCCESS(seq);
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::parameter(Pos& pos) {
		/*
			parameter := identifier (AS identifier)? (':' operation)?;
		*/
		Pos p = pos;
		ASTNode* id = identifier(p);
		if (!id) MATCH_FAILED();
		ASTNode* type_id = NULL;
		if (p->type == Token::AS) {
			++p;
			type_id = identifier(p);
			if (!type_id) {
				error(p, "Expected type identifier for AS keyword, got %s.", get_token_name(p->type));
				MATCH_FAILED();
			}
		}
		
		ASTNode* default_expr = NULL;
		if (p->type == Token::ASSIGN) {
			++p;
			default_expr = operation(p);
			if (!default_expr) {
				error(p, "Expected expression for default parameter value, got %s.", get_token_name(p->type));
				MATCH_FAILED();
			}
		}
		
		pos = p;
		Symbol name = id->identifier.name;
		ast->free(id);
		MATCH_SUCCESS(ast->parameter(name, type_id, default_expr));
	}
	
	ASTNode* Parser::identifier(Pos& pos) {
		/*
			identifier := IDENTIFIER;
		*/
		if (pos->type == Token::IDENTIFIER) {
			GET_TOKEN_SZ(data, pos);
			++pos;
			MATCH_SUCCESS(ast->identifier(snow::sym(data)));
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::literal(Pos& pos) {
		/*
			literal := nil | true | false | INTEGER | FLOAT | DQSTRING | SQSTRING | symbol;
		*/
		switch (pos->type) {
			case Token::NIL: { ++pos; MATCH_SUCCESS(ast->literal(SN_NIL)); }
			case Token::TRUE: { ++pos; MATCH_SUCCESS(ast->literal(SN_TRUE)); }
			case Token::FALSE: { ++pos; MATCH_SUCCESS(ast->literal(SN_FALSE)); }
			case Token::INTEGER: {
				GET_TOKEN_SZ(data, pos);
				long long n = atoll(data);
				++pos;
				MATCH_SUCCESS(ast->literal(integer_to_value(n)));
			}
			case Token::FLOAT: {
				GET_TOKEN_SZ(data, pos);
				float f = strtof(data, NULL);
				++pos;
				MATCH_SUCCESS(ast->literal(float_to_value(f)));
			}
			case Token::DQSTRING:
				// TODO: Interpolation, escapes
			case Token::SQSTRING: {
				SnObject* str = create_string_with_size(pos->begin, pos->length);
				++pos;
				MATCH_SUCCESS(ast->literal(str));
			}
			default:
				return symbol(pos);
		}
	}
	
	ASTNode* Parser::symbol(Pos& pos) {
		/*
			symbol := '#' (identifier | SQSTRING | DQSTRING)
		*/
		
		if (pos->type == Token::HASH) {
			++pos;
			ASTNode* id = identifier(pos);
			if (id) {
				ASTNode* sym = ast->literal(snow::symbol_to_value(id->identifier.name));
				ast->free(id);
				MATCH_SUCCESS(sym);
			}
			
			if (pos->type == Token::SQSTRING || pos->type == Token::DQSTRING) {
				// TODO: Run-time conversion
				GET_TOKEN_SZ(data, pos);
				++pos;
				MATCH_SUCCESS(ast->literal(vsym(data)));
			}
			
			if (is_operator(pos->type)) {
				GET_TOKEN_SZ(data, pos);
				++pos;
				MATCH_SUCCESS(ast->literal(vsym(data)));
			}
			
			MATCH_SUCCESS(ast->identifier(snow::sym("#")));
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::self(Pos& pos) {
		if (pos->type == Token::SELF) {
			++pos;
			MATCH_SUCCESS(ast->self());
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::it(Pos& pos) {
		if (pos->type == Token::IT) {
			++pos;
			MATCH_SUCCESS(ast->it());
		}
		MATCH_FAILED();
	}
	
	ASTNode* Parser::here(Pos& pos) {
		if (pos->type == Token::HERE) {
			++pos;
			MATCH_SUCCESS(ast->here());
		}
		MATCH_FAILED();
	}
	
	
	ASTNode* Parser::parse_post_terms(Pos& pos, ASTNode* expr) {
		while (true) {
			ASTNode* result = NULL;
			FIRST_MATCH({
				MATCH(postcondition);
				MATCH(postloop);
			});
			if (result) {
				if (result->type == ASTNodeTypeIfElse) {
					result->if_else.body = expr;
					expr = result;
				} else if (result->type == ASTNodeTypeLoop) {
					result->loop.body = expr;
					expr = result;
				} else {
					error(pos, "Rules postcondition or postloop return unknown AST node.");
					MATCH_FAILED();
				}
			} else
				break;
		}
		return expr;
	}
	
	bool Parser::skip_end_of_statement(Pos& pos) {
		switch (pos->type) {
			case Token::END_OF_LINE:
			case Token::END_OF_STATEMENT:
				++pos;
				return true;
			default:
				return false;
		}
	}
	
	ASTNode* Parser::reduce_operation(ASTNode* a, ASTNode* b, Pos op) {
		switch (op->type) {
			case Token::LOG_AND: return ast->logic_and(a, b);
			case Token::LOG_OR:  return ast->logic_or(a, b);
			case Token::LOG_XOR: return ast->logic_xor(a, b);
			default: {
				GET_TOKEN_SZ(op_str, op);
				return ast->call(ast->method(a, snow::sym(op_str)), ast->sequence(1, b));
			}
		}
	}
	
	ASTBase* parse(const char* buffer) {
		Lexer l(buffer);
		l.tokenize();
		Parser parser(l.begin());
		AST* ast = parser.parse();
		//ast->print();
		return ast;
	}
}

