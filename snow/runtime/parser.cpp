#include "snow/parser.h"
#include "snow/symbol.h"
#include "snow/numeric.h"
#include "snow/str.h"

#include "linkheap.hpp"
#include "ast-intern.hpp"
#include "lexer.hpp"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

#include <vector>
#include <bits/stl_pair.h>

#define GET_TOKEN_SZ(SZ, TOKEN) char* SZ = (char*)alloca((TOKEN)->length+1); memcpy(SZ, (TOKEN)->begin, (TOKEN)->length); (SZ)[(TOKEN)->length] = '\0'

#define FIRST_MATCH(BLOCK) do BLOCK while(0)
#define MATCH(RULE) { Pos p = pos; SnAstNode* r = RULE(p); if (r) { result = r; pos = p; break; } }

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
		
		Parser(SN_P p, Pos pos) : _p(p), _pos(pos), _error_message(NULL) {}
		~Parser() { free(_error_message); }
		
		AST* parse();
		
	private:
		void error(Pos error_pos, const char* message, ...);
		
		SnAstNode* sequence(Pos&);
		SnAstNode* statement(Pos&);
		SnAstNode* expression(Pos&);
		SnAstNode* expression_no_assign(Pos&);
		SnAstNode* control(Pos&);
		SnAstNode* operation(Pos&);
		SnAstNode* operand(Pos&);
		SnAstNode* operand_with_unary(Pos&);
		SnAstNode* assignment(Pos&);
		SnAstNode* assign_target(Pos&);
		SnAstNode* assign_values(Pos&);
		SnAstNode* lvalue(Pos&);
		SnAstNode* lvalues(Pos&);
		SnAstNode* postcondition(Pos&);
		SnAstNode* postloop(Pos&);
		SnAstNode* condition(Pos&);
		SnAstNode* loop(Pos&);
		SnAstNode* atomic_expression(Pos&);
		SnAstNode* chain(Pos&);
		SnAstNode* chainable(Pos&);
		SnAstNode* call(Pos&);
		SnAstNode* member(Pos&);
		SnAstNode* argument_list(Pos&);
		SnAstNode* argument(Pos&);
		SnAstNode* named_argument(Pos&);
		SnAstNode* association_key_list(Pos&);
		SnAstNode* closure(Pos&);
		SnAstNode* parameter_list(Pos&);
		SnAstNode* parameter(Pos&);
		SnAstNode* identifier(Pos&);
		SnAstNode* literal(Pos&);
		SnAstNode* symbol(Pos&);
		SnAstNode* self(Pos&);
		SnAstNode* it(Pos&);
		SnAstNode* here(Pos&);
		
		SnAstNode* parse_post_terms(Pos&, SnAstNode* for_expr); // postconditions and postloops
		bool skip_end_of_statement(Pos&);
		
		SnAstNode* reduce_operation(SnAstNode* a, SnAstNode* b, Pos op);
	private:
		class PrecedenceParser;
		friend class PrecedenceParser;
		
		SN_P _p;
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
		std::vector<SnAstNode*> operands;
		
		PrecedenceParser(Parser* parser, SnAstNode* first) : parser(parser) {
			operands.push_back(first);
			operators.push_back(Op(Token::INVALID, Parser::Pos()));
		}
		
		void push_operand(SnAstNode* a) {
			operands.push_back(a);
		}
		
		void push_operator(const Parser::Pos& pos) {
			while (operators.back().first >= pos->type) { // '>=' means left-associative -- > would mean right-associative
				pop_operator();
			}
			operators.push_back(Op(pos->type, pos));
		}
		void pop_operator() {
			SnAstNode* b = operands.back();
			operands.pop_back();
			SnAstNode* a = operands.back();
			operands.pop_back();
			Parser::Pos op = operators.back().second;
			operators.pop_back();
			
			SnAstNode* reduced = parser->reduce_operation(a, b, op);
			operands.push_back(reduced);
		}
		
		SnAstNode* reduce() {
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
			SnAstNode* seq = sequence(_pos);
			if (_pos->type != Token::END_OF_FILE) {
				error(_pos, "Expected END_OF_FILE, got '%s'!", get_token_name(_pos->type));
			}
			ast->set_root(seq);
			return ast;
		} else {
			fprintf(stderr, "PARSER ERROR: %s\n", _error_message);
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
	
	SnAstNode* Parser::sequence(Pos& pos) {
		/*
			sequence := separator* statement*;
		*/
		while (skip_end_of_statement(pos));
		SnAstNode* seq = ast->sequence();
		while (!pos.at_end()) {
			SnAstNode* s = statement(pos);
			if (s) ast->sequence_push(seq, s);
			else break;
		}
		MATCH_SUCCESS(seq);
	}
	
	SnAstNode* Parser::statement(Pos& pos) {
		/*
			statement := (expression | control) separator*;
		*/
		SnAstNode* result = NULL;
		FIRST_MATCH({
			MATCH(expression);
			MATCH(control);
		});
		if (result) { while (skip_end_of_statement(pos)); }
		MATCH_SUCCESS(result);
	}
	
	SnAstNode* Parser::expression(Pos& pos) {
		/*
			expression := (assignment | operation) (postcondition | postloop)*;
		*/
		SnAstNode* result = NULL;
		FIRST_MATCH({
			MATCH(assignment);
			MATCH(operation);
		});
		if (!result) { MATCH_FAILED(); }
		
		result = parse_post_terms(pos, result);
		MATCH_SUCCESS(result);
	}
	
	SnAstNode* Parser::expression_no_assign(Pos& pos) {
		/*
			expression_no_assign := operation (postcondition | postloop)*;
		*/
		SnAstNode* expr = operation(pos);
		if (expr) {
			expr = parse_post_terms(pos, expr);
			MATCH_SUCCESS(expr);
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::control(Pos& pos) {
		/*
			control := (return | BREAK | CONTINUE) postcondition?;
			return := RETURN operand;
		*/
		SnAstNode* result = NULL;
		if (pos->type == Token::BREAK) { result = ast->break_(); ++pos; }
		else if (pos->type == Token::CONTINUE) { result = ast->continue_(); ++pos; }
		else if (pos->type == Token::RETURN) {
			++pos;
			SnAstNode* v = operand(pos);
			result = ast->return_(v);
		}
		
		if (result) {
			SnAstNode* postcond = postcondition(pos);
			if (postcond) {
				postcond->if_else.body = result;
				result = postcond;
			}
			MATCH_SUCCESS(result);
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::operation(Pos& pos) {
		/*
			operation := operand (operator operand)*;
		*/
		Pos p = pos;
		SnAstNode* a = operand_with_unary(p);
		
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
	
	SnAstNode* Parser::operand(Pos& pos) {
		/*
			operand :=  operator? (condition | loop | chain | atomic_expression);
		*/
		
		SnAstNode* result = NULL;
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
	
	SnAstNode* Parser::operand_with_unary(Pos& pos) {
		Pos p = pos;
		if (is_operator(p->type)) {
			GET_TOKEN_SZ(data, p);
			++p;
			SnAstNode* a = operand(p);
			if (!a) {
				error(p, "Expected operand for unary operator, got %s.", get_token_name(p->type));
				MATCH_FAILED();
			}
			pos = p;
			return ast->call(ast->member(a, snow_sym(_p, data)), NULL);
		}
		return operand(pos);
	}
	
	SnAstNode* Parser::assign_target(Pos& pos) {
		Pos p = pos;
		SnAstNode* lvs = lvalues(p);
		if (lvs && p->type == Token::ASSIGN) {
			pos = ++p;
			MATCH_SUCCESS(ast->assign(lvs, NULL));
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::assign_values(Pos& pos) {
		Pos p = pos;
		SnAstNode* a = operation(p);
		if (a) {
			SnAstNode* seq = ast->sequence(1, a);
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
	
	SnAstNode* Parser::assignment(Pos& pos) {
		/*
			assignment := (lvalues ':')+ operation (',' operation)*;
		*/
		Pos p = pos;
		SnAstNode* result = assign_target(p);
		if (result) {
			SnAstNode* last_assign = result;
			SnAstNode* assign = assign_target(p);
			while (assign) {
				last_assign->assign.value = assign;
				last_assign = assign;
				assign = assign_target(p);
			}
			
			SnAstNode* values = assign_values(p);
			if (values) {
				last_assign->assign.value = values;
			} else {
				error(p, "Unexpected token %s.", get_token_name(p->type));
			}
		}
		if (result) { pos = p; MATCH_SUCCESS(result); }
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::lvalue(Pos& pos) {
		/*
			lvalue := association | member | identifier;
			
			TODO:
			lvalue := chain_that_doesnt_end_with_call | identifier;
		*/
		SnAstNode* result = chain(pos);
		if (result) MATCH_SUCCESS(result);
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::lvalues(Pos& pos) {
		/*
			lvalues := lvalue (',' lvalue)*;
		*/
		
		SnAstNode* lv = lvalue(pos);
		if (lv) {
			SnAstNode* seq = ast->sequence(1, lv);
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
	
	SnAstNode* Parser::postcondition(Pos& pos) {
		Token::Type type = pos->type;
		if (type == Token::IF || type == Token::UNLESS) {
			++pos;
			SnAstNode* op = operation(pos);
			if (!op) {
				error(pos, "Expected expression for postcondition, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			if (type == Token::UNLESS) {
				op = ast->logic_not(op);
			}
			SnAstNode* cond = ast->if_else(op, NULL, NULL);
			MATCH_SUCCESS(cond);
		}
		MATCH_FAILED();
	}
	SnAstNode* Parser::postloop(Pos& pos) {
		/*
			postloop := (WHILE | UNTIL) operation;
		*/
		Token::Type type = pos->type;
		if (type == Token::WHILE || type == Token::UNTIL) {
			++pos;
			SnAstNode* op = operation(pos);
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
	
	SnAstNode* Parser::condition(Pos& pos) {
		// TODO!!
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::loop(Pos& pos) {
		/*
			loop := (WHILE | UNTIL) operation (DO | separator) sequence END;
		*/
		Token::Type type = pos->type;
		if (type == Token::WHILE || type == Token::UNTIL) {
			SnAstNode* op = operation(pos);
			if (!op) {
				error(pos, "Expected expression for loop condition, got %s.", get_token_name(pos->type));
				MATCH_FAILED();
			}
			
			if (pos->type == Token::DO || skip_end_of_statement(pos)) {
				while (skip_end_of_statement(pos)); // skip additional ends
				SnAstNode* body = sequence(pos);
				if (pos->type == Token::END) {
					++pos;
					if (type == Token::UNTIL) { op = ast->logic_not(op); }
					SnAstNode* loop = ast->loop(op, body);
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
	
	SnAstNode* Parser::atomic_expression(Pos& pos) {
		/*
			atomic_expression := literal | self | it | here | identifier | closure | '(' expression ')';
		*/
		
		SnAstNode* result = NULL;
		FIRST_MATCH({
			MATCH(literal);
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
	
	SnAstNode* Parser::chain(Pos& pos) {
		/*
			chain := (call | member | association)+;
		*/
		Pos p = pos;
		SnAstNode* result = atomic_expression(p);
		if (!result && p->type == Token::DOT)
			result = ast->self();
		
		bool keep_going = true;
		while (keep_going) {
			switch (p->type) {
				case Token::DOT: {
					++p;
					SnAstNode* id = identifier(p);
					if (!id) {
						error(p, "Expected identifier as right hand of dot operator, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					SnSymbol sym = id->identifier.name;
					ast->free(id);
					result = ast->member(result, sym);
					break;
				}
				case Token::BRACE_BEGIN:
					// closure call without parameters
				case Token::PARENS_BEGIN: {
					// regular call
					SnAstNode* arg_list = argument_list(p);
					if (!arg_list) {
						error(p, "Expected argument list for method call, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					result = ast->call(result, arg_list);
					break;
				}
				case Token::BRACKET_BEGIN: {
					// either association, or closure call with parameters
					SnAstNode* block = closure(p);
					if (block) {
						result = ast->call(result, ast->sequence(1, block));
						break;
					}
					SnAstNode* assoc_key = association_key_list(p);
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
	
	SnAstNode* Parser::argument_list(Pos& pos) {
		/*
			argument_list := closure | ('(' (argument (',' argument)*)? ')' closure?);
		*/
		SnAstNode* result = closure(pos);
		if (result) MATCH_SUCCESS(ast->sequence(1, result));
		
		Pos p = pos;
		if (p->type == Token::PARENS_BEGIN) {
			++p;
			SnAstNode* seq = ast->sequence();
			bool first = true;
			while (p->type != Token::PARENS_END) {
				if (!first) {
					if (p->type != Token::COMMA) {
						error(p, "Expected COMMA in function argument list, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					++p;
				}
				first = false;
				
				SnAstNode* arg = argument(p);
				if (!arg) {
					error(p, "Invalid function argument, got %s.", get_token_name(p->type));
					MATCH_FAILED();
				}
				ast->sequence_push(seq, arg);
			}
			
			ASSERT(p->type == Token::PARENS_END);
			++p; // skip PARENS_END
			
			SnAstNode* block = closure(p);
			if (block) ast->sequence_push(seq, block);
			pos = p;
			MATCH_SUCCESS(seq);
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::argument(Pos& pos) {
		/*
			argument := named_argument | (operation (postcondition | postloop)*));
		*/
		SnAstNode* named_arg = named_argument(pos);
		if (named_arg) MATCH_SUCCESS(named_arg);
		
		SnAstNode* expr = operation(pos);
		if (expr) {
			expr = parse_post_terms(pos, expr);
			MATCH_SUCCESS(expr);
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::named_argument(Pos& pos) {
		/*
			named_argument := identifier ':' expression_no_assign;
		*/
		Pos p = pos;
			
		SnAstNode* id = identifier(p);
		if (!id) MATCH_FAILED();
		if (p->type != Token::ASSIGN) MATCH_FAILED();
		++p;
		SnAstNode* expr = expression_no_assign(p);
		if (expr) {
			SnSymbol name = id->identifier.name;
			ast->free(id);
			pos = p;
			MATCH_SUCCESS(ast->named_argument(name, expr));
		}
		error(p, "Expected right-hand side for named argument, got %s.", get_token_name(p->type));
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::association_key_list(Pos& pos) {
		/*
			association := association_target ('[' expression_no_assign (',' expression_no_assign)* ']')+
		*/
		if (pos->type == Token::BRACKET_BEGIN) {
			++pos;
			bool first = true;
			SnAstNode* seq = ast->sequence();
			
			while (pos->type != Token::BRACKET_END) {
				if (!first) {
					if (pos->type != Token::COMMA) {
						error(pos, "Expected COMMA or BRACKET_END in association key list, got %s.", get_token_name(pos->type));
						MATCH_FAILED();
					}
					++pos;
				}
				first = false;
				
				SnAstNode* expr = expression_no_assign(pos);
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
	
	SnAstNode* Parser::closure(Pos& pos) {
		/*
			closure := parameter_list? '{' sequence '}';
		*/
		Pos p = pos;
		SnAstNode* params = parameter_list(p);
		if (p->type == Token::BRACE_BEGIN) {
			// TODO: Save beginning position to give better error messages.
			++p;
			SnAstNode* body = sequence(p);
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
	
	SnAstNode* Parser::parameter_list(Pos& pos) {
		/*
			parameter_list := '[' (parameter (',' parameter)*)? ']';
		*/
		Pos p = pos;
		if (p->type == Token::BRACKET_BEGIN) {
			++p;
			SnAstNode* seq = ast->sequence();
			
			bool first = true;
			while (p->type != Token::BRACKET_END) {
				if (!first) {
					if (!p->type != Token::COMMA) {
						error(p, "Expected COMMA in parameter list, got %s.", get_token_name(p->type));
						MATCH_FAILED();
					}
					++p;
				}
				first = false;
				
				SnAstNode* param = parameter(p);
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
	
	SnAstNode* Parser::parameter(Pos& pos) {
		/*
			parameter := identifier (AS identifier)? (':' operation)?;
		*/
		Pos p = pos;
		SnAstNode* id = identifier(p);
		if (!id) MATCH_FAILED();
		SnAstNode* type_id = NULL;
		if (p->type == Token::AS) {
			++p;
			type_id = identifier(p);
			if (!type_id) {
				error(p, "Expected type identifier for AS keyword, got %s.", get_token_name(p->type));
				MATCH_FAILED();
			}
		}
		
		SnAstNode* default_expr = NULL;
		if (p->type == Token::ASSIGN) {
			++p;
			default_expr = operation(p);
			if (!default_expr) {
				error(p, "Expected expression for default parameter value, got %s.", get_token_name(p->type));
				MATCH_FAILED();
			}
		}
		
		pos = p;
		SnSymbol name = id->identifier.name;
		ast->free(id);
		MATCH_SUCCESS(ast->parameter(name, type_id, default_expr));
	}
	
	SnAstNode* Parser::identifier(Pos& pos) {
		/*
			identifier := IDENTIFIER;
		*/
		if (pos->type == Token::IDENTIFIER) {
			GET_TOKEN_SZ(data, pos);
			++pos;
			MATCH_SUCCESS(ast->identifier(snow_sym(_p, data)));
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::literal(Pos& pos) {
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
				MATCH_SUCCESS(ast->literal(snow_integer_to_value(n)));
			}
			case Token::FLOAT: {
				GET_TOKEN_SZ(data, pos);
				float f = strtof(data, NULL);
				++pos;
				MATCH_SUCCESS(ast->literal(snow_float_to_value(f)));
			}
			case Token::DQSTRING:
				// TODO: Interpolation, escapes
			case Token::SQSTRING: {
				SnStringRef str = snow_create_string_with_size(_p, pos->begin, pos->length);
				++pos;
				MATCH_SUCCESS(ast->literal(snow_string_as_object(str)));
			}
			default:
				return symbol(pos);
		}
	}
	
	SnAstNode* Parser::symbol(Pos& pos) {
		/*
			symbol := '#' (identifier | SQSTRING | DQSTRING)
		*/
		
		if (pos->type == Token::HASH) {
			++pos;
			SnAstNode* id = identifier(pos);
			if (id) {
				SnAstNode* sym = ast->literal(snow_symbol_to_value(id->identifier.name));
				ast->free(id);
				MATCH_SUCCESS(sym);
			}
			
			if (pos->type == Token::SQSTRING || pos->type == Token::DQSTRING) {
				// TODO: Run-time conversion
				GET_TOKEN_SZ(data, pos);
				++pos;
				MATCH_SUCCESS(ast->literal(snow_vsym(_p, data)));
			}
			
			error(pos, "Expected identifier or string following symbol initiator '#', got %s.", get_token_name(pos->type));
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::self(Pos& pos) {
		if (pos->type == Token::SELF) {
			++pos;
			MATCH_SUCCESS(ast->self());
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::it(Pos& pos) {
		if (pos->type == Token::IT) {
			++pos;
			MATCH_SUCCESS(ast->it());
		}
		MATCH_FAILED();
	}
	
	SnAstNode* Parser::here(Pos& pos) {
		if (pos->type == Token::HERE) {
			++pos;
			MATCH_SUCCESS(ast->here());
		}
		MATCH_FAILED();
	}
	
	
	SnAstNode* Parser::parse_post_terms(Pos& pos, SnAstNode* expr) {
		while (true) {
			SnAstNode* result = NULL;
			FIRST_MATCH({
				MATCH(postcondition);
				MATCH(postloop);
			});
			if (result) {
				if (result->type == SN_AST_IF_ELSE) {
					result->if_else.body = expr;
					expr = result;
				} else if (result->type == SN_AST_LOOP) {
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
	
	SnAstNode* Parser::reduce_operation(SnAstNode* a, SnAstNode* b, Pos op) {
		GET_TOKEN_SZ(op_str, op);
		return ast->call(ast->member(a, snow_sym(_p, op_str)), ast->sequence(1, b));
	}
}


CAPI SnAST* snow_parse(SN_P p, const char* buffer) {
	printf("LEXING\n");
	snow::Lexer l(buffer);
	l.tokenize();
	printf("PARSING\n");
	snow::Parser parser(p, l.begin());
	snow::AST* ast = parser.parse();
	printf("PRINTING AST\n");
	ast->print();
	return ast;
}