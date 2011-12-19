#pragma once
#ifndef AST_INTERN_HPP_UNJFFI95
#define AST_INTERN_HPP_UNJFFI95

#include "snow/ast.hpp"
#include "linkheap.hpp"

namespace snow {
	class AST : public ASTBase {
	public:
		ASTNode* root() const { return _root; }
		void set_root(ASTNode* n) { _root = n; }
		void clear();
		void print(ASTNode* n = NULL, int indent = 0) const; // NULL means start from root
		void free(ASTNode* n, bool recursive = true);

		ASTNode* sequence(unsigned int n = 0, ...);
		void sequence_push(ASTNode* seq, ASTNode* n);
		ASTNode* literal(VALUE val);
		ASTNode* closure(ASTNode* parameters, ASTNode* body);
		ASTNode* parameter(Symbol name, ASTNode* id_type, ASTNode* default_value);
		ASTNode* return_(ASTNode* value = NULL);
		ASTNode* identifier(Symbol sym);
		ASTNode* break_() { return create(SN_AST_BREAK); }
		ASTNode* continue_() { return create(SN_AST_CONTINUE); }
		ASTNode* self() { return create(SN_AST_SELF); }
		ASTNode* here() { return create(SN_AST_HERE); }
		ASTNode* it() { return create(SN_AST_IT); }
		ASTNode* assign(ASTNode* target, ASTNode* value);
		ASTNode* method(ASTNode* object, Symbol sym);
		ASTNode* instance_variable(ASTNode* object, Symbol sym);
		ASTNode* call(ASTNode* object, ASTNode* arguments);
		ASTNode* association(ASTNode* object, ASTNode* arguments);
		ASTNode* named_argument(Symbol name, ASTNode* expr);
		ASTNode* logic_and(ASTNode* left, ASTNode* right);
		ASTNode* logic_or(ASTNode* left, ASTNode* right);
		ASTNode* logic_xor(ASTNode* left, ASTNode* right);
		ASTNode* logic_not(ASTNode* expr);
		ASTNode* loop(ASTNode* cond, ASTNode* body);
		ASTNode* if_else(ASTNode* cond, ASTNode* body, ASTNode* else_body);
	private:
		LinkHeap<ASTNode> _heap;

		ASTNode* create(ASTNodeType type);
		void print_r(ASTNode* n, int indent) const;
	};

	inline ASTNode* AST::sequence(unsigned int n, ...) {
		ASTNode* seq = create(SN_AST_SEQUENCE);
		seq->sequence.length = 0;
		seq->sequence.head = seq->sequence.tail = NULL;
		
		va_list ap;
		va_start(ap, n);
		for (int i = 0; i < n; ++i) {
			sequence_push(seq, va_arg(ap, ASTNode*));
		}
		va_end(ap);
		
		return seq;
	}
	
	inline void AST::sequence_push(ASTNode* seq, ASTNode* n) {
		ASSERT(seq->type == SN_AST_SEQUENCE);
		++seq->sequence.length;
		if (!seq->sequence.head) { seq->sequence.head = seq->sequence.tail = n; }
		else { seq->sequence.tail->next = n; seq->sequence.tail = n; }
	}
	
	inline ASTNode* AST::literal(VALUE val) {
		ASTNode* n = create(SN_AST_LITERAL);
		n->literal.value = val;
		return n;
	}
	
	inline ASTNode* AST::closure(ASTNode* parameters, ASTNode* body) {
		ASTNode* n = create(SN_AST_CLOSURE);
		n->closure.parameters = parameters; n->closure.body = body;
		return n;
	}
	
	inline ASTNode* AST::parameter(Symbol name, ASTNode* id_type, ASTNode* default_value) {
		ASTNode* n = create(SN_AST_PARAMETER);
		n->parameter.name = name;
		n->parameter.id_type = id_type;
		n->parameter.default_value = default_value;
		return n;
	}
	
	inline ASTNode* AST::return_(ASTNode* value) {
		ASTNode* n = create(SN_AST_RETURN);
		n->return_expr.value = value;
		return n;
	}
	
	inline ASTNode* AST::identifier(Symbol sym) {
		ASTNode* n = create(SN_AST_IDENTIFIER);
		n->identifier.name = sym;
		return n;
	}
	
	inline ASTNode* AST::assign(ASTNode* target, ASTNode* value) {
		ASTNode* n = create(SN_AST_ASSIGN);
		n->assign.target = target;
		n->assign.value = value;
		return n;
	}
	
	inline ASTNode* AST::method(ASTNode* object, Symbol sym) {
		ASTNode* n = create(SN_AST_METHOD);
		n->method.object = object;
		n->method.name = sym;
		return n;
	}
	
	inline ASTNode* AST::instance_variable(ASTNode* object, Symbol sym) {
		ASTNode* n = create(SN_AST_INSTANCE_VARIABLE);
		n->instance_variable.object = object;
		n->instance_variable.name = sym;
		return n;
	}
	
	inline ASTNode* AST::call(ASTNode* object, ASTNode* arguments) {
		ASTNode* n = create(SN_AST_CALL);
		n->call.object = object;
		n->call.args = arguments;
		return n;
	}
	
	inline ASTNode* AST::association(ASTNode* object, ASTNode* arguments) {
		ASTNode* n = create(SN_AST_ASSOCIATION);
		n->association.object = object;
		n->association.args = arguments;
		return n;
	}
	
	inline ASTNode* AST::named_argument(Symbol name, ASTNode* expr) {
		ASTNode* n = create(SN_AST_NAMED_ARGUMENT);
		n->named_argument.name = name;
		n->named_argument.expr = expr;
		return n;
	}
	
	inline ASTNode* AST::logic_and(ASTNode* left, ASTNode* right) {
		ASTNode* n = create(SN_AST_AND);
		n->logic_and.left = left;
		n->logic_and.right = right;
		return n;
	}
	
	inline ASTNode* AST::logic_or(ASTNode* left, ASTNode* right) {
		ASTNode* n = logic_and(left, right); n->type = SN_AST_OR;
		return n;
	}
	
	inline ASTNode* AST::logic_xor(ASTNode* left, ASTNode* right) {
		ASTNode* n = logic_and(left, right); n->type = SN_AST_XOR;
		return n;
	}
	
	inline ASTNode* AST::logic_not(ASTNode* expr) {
		ASTNode* n = create(SN_AST_NOT);
		n->logic_not.expr = expr;
		return n;
	}
	
	inline ASTNode* AST::loop(ASTNode* cond, ASTNode* body) {
		ASTNode* n = create(SN_AST_LOOP);
		n->loop.cond = cond;
		n->loop.body = body;
		return n;
	}
	
	inline ASTNode* AST::if_else(ASTNode* cond, ASTNode* body, ASTNode* else_body) {
		ASTNode* n = create(SN_AST_IF_ELSE);
		n->if_else.cond = cond;
		n->if_else.body = body;
		n->if_else.else_body = else_body;
		return n;
	}

	inline ASTNode* AST::create(ASTNodeType type) {
		ASTNode* n = _heap.alloc();
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
