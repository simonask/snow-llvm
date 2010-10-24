#pragma once
#ifndef AST_INTERN_HPP_UNJFFI95
#define AST_INTERN_HPP_UNJFFI95

#include "snow/ast.hpp"
#include "linkheap.hpp"

namespace snow {
	class AST : public SnAST {
	public:
		SnAstNode* root() const { return _root; }
		void set_root(SnAstNode* n) { _root = n; }
		void clear();
		void print(SnAstNode* n = NULL, int indent = 0) const; // NULL means start from root
		void free(SnAstNode* n, bool recursive = true);

		SnAstNode* sequence(unsigned int n = 0, ...);
		void sequence_push(SnAstNode* seq, SnAstNode* n);
		SnAstNode* literal(VALUE val);
		SnAstNode* closure(SnAstNode* parameters, SnAstNode* body);
		SnAstNode* parameter(SnSymbol name, SnAstNode* id_type, SnAstNode* default_value);
		SnAstNode* return_(SnAstNode* value = NULL);
		SnAstNode* identifier(SnSymbol sym);
		SnAstNode* break_() { return create(SN_AST_BREAK); }
		SnAstNode* continue_() { return create(SN_AST_CONTINUE); }
		SnAstNode* self() { return create(SN_AST_SELF); }
		SnAstNode* here() { return create(SN_AST_HERE); }
		SnAstNode* it() { return create(SN_AST_IT); }
		SnAstNode* assign(SnAstNode* target, SnAstNode* value);
		SnAstNode* member(SnAstNode* object, SnSymbol sym);
		SnAstNode* call(SnAstNode* object, SnAstNode* arguments);
		SnAstNode* association(SnAstNode* object, SnAstNode* arguments);
		SnAstNode* named_argument(SnSymbol name, SnAstNode* expr);
		SnAstNode* logic_and(SnAstNode* left, SnAstNode* right);
		SnAstNode* logic_or(SnAstNode* left, SnAstNode* right);
		SnAstNode* logic_xor(SnAstNode* left, SnAstNode* right);
		SnAstNode* logic_not(SnAstNode* expr);
		SnAstNode* loop(SnAstNode* cond, SnAstNode* body);
		SnAstNode* if_else(SnAstNode* cond, SnAstNode* body, SnAstNode* else_body);
	private:
		SnAstNode* _root;
		LinkHeap<SnAstNode> _heap;

		SnAstNode* create(SnSnAstNodeType type);
		void print_r(SnAstNode* n, int indent) const;
	};

	inline SnAstNode* AST::sequence(unsigned int n, ...) {
		SnAstNode* seq = create(SN_AST_SEQUENCE);
		seq->sequence.head = seq->sequence.tail = NULL;
		
		va_list ap;
		va_start(ap, n);
		for (int i = 0; i < n; ++i) {
			sequence_push(seq, va_arg(ap, SnAstNode*));
		}
		va_end(ap);
		
		return seq;
	}
	
	inline void AST::sequence_push(SnAstNode* seq, SnAstNode* n) {
		ASSERT(seq->type == SN_AST_SEQUENCE);
		if (!seq->sequence.head) { seq->sequence.head = seq->sequence.tail = n; }
		else { seq->sequence.tail->next = n; seq->sequence.tail = n; }
	}
	
	inline SnAstNode* AST::literal(VALUE val) {
		SnAstNode* n = create(SN_AST_LITERAL);
		n->literal.value = val;
		return n;
	}
	
	inline SnAstNode* AST::closure(SnAstNode* parameters, SnAstNode* body) {
		SnAstNode* n = create(SN_AST_CLOSURE);
		n->closure.parameters = parameters; n->closure.body = body;
		return n;
	}
	
	inline SnAstNode* AST::parameter(SnSymbol name, SnAstNode* id_type, SnAstNode* default_value) {
		SnAstNode* n = create(SN_AST_PARAMETER);
		n->parameter.name = name;
		n->parameter.id_type = id_type;
		n->parameter.default_value = default_value;
		return n;
	}
	
	inline SnAstNode* AST::return_(SnAstNode* value) {
		SnAstNode* n = create(SN_AST_RETURN);
		n->return_expr.value = value;
		return n;
	}
	
	inline SnAstNode* AST::identifier(SnSymbol sym) {
		SnAstNode* n = create(SN_AST_IDENTIFIER);
		n->identifier.name = sym;
		return n;
	}
	
	inline SnAstNode* AST::assign(SnAstNode* target, SnAstNode* value) {
		SnAstNode* n = create(SN_AST_ASSIGN);
		n->assign.target = target;
		n->assign.value = value;
		return n;
	}
	
	inline SnAstNode* AST::member(SnAstNode* object, SnSymbol sym) {
		SnAstNode* n = create(SN_AST_MEMBER);
		n->member.object = object;
		n->member.name = sym;
		return n;
	}
	
	inline SnAstNode* AST::call(SnAstNode* object, SnAstNode* arguments) {
		SnAstNode* n = create(SN_AST_CALL);
		n->call.object = object;
		n->call.args = arguments;
		return n;
	}
	
	inline SnAstNode* AST::association(SnAstNode* object, SnAstNode* arguments) {
		SnAstNode* n = create(SN_AST_ASSOCIATION);
		n->association.object = object;
		n->association.args = arguments;
		return n;
	}
	
	inline SnAstNode* AST::named_argument(SnSymbol name, SnAstNode* expr) {
		SnAstNode* n = create(SN_AST_NAMED_ARGUMENT);
		n->named_argument.name = name;
		n->named_argument.expr = expr;
		return n;
	}
	
	inline SnAstNode* AST::logic_and(SnAstNode* left, SnAstNode* right) {
		SnAstNode* n = create(SN_AST_AND);
		n->logic_and.left = left;
		n->logic_and.right = right;
		return n;
	}
	
	inline SnAstNode* AST::logic_or(SnAstNode* left, SnAstNode* right) {
		SnAstNode* n = logic_and(left, right); n->type = SN_AST_OR;
		return n;
	}
	
	inline SnAstNode* AST::logic_xor(SnAstNode* left, SnAstNode* right) {
		SnAstNode* n = logic_and(left, right); n->type = SN_AST_XOR;
		return n;
	}
	
	inline SnAstNode* AST::logic_not(SnAstNode* expr) {
		SnAstNode* n = create(SN_AST_NOT);
		n->logic_not.expr = expr;
		return n;
	}
	
	inline SnAstNode* AST::loop(SnAstNode* cond, SnAstNode* body) {
		SnAstNode* n = create(SN_AST_LOOP);
		n->loop.cond = cond;
		n->loop.body = body;
		return n;
	}
	
	inline SnAstNode* AST::if_else(SnAstNode* cond, SnAstNode* body, SnAstNode* else_body) {
		SnAstNode* n = create(SN_AST_IF_ELSE);
		n->if_else.cond = cond;
		n->if_else.body = body;
		n->if_else.else_body = else_body;
		return n;
	}

	inline SnAstNode* AST::create(SnSnAstNodeType type) {
		SnAstNode* n = _heap.alloc();
		n->type = type;
		n->next = NULL;
		return n;
	}
	
	inline void AST::clear() {
		_heap.clear();
		_root = NULL;
	}
}

#endif /* end of include guard: AST_INTERN_HPP_UNJFFI95 */
