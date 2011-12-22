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
		ASTNode* literal(const Immediate& val);
		ASTNode* closure(ASTNode* parameters, ASTNode* body);
		ASTNode* parameter(Symbol name, ASTNode* id_type, ASTNode* default_value);
		ASTNode* return_(ASTNode* value = NULL);
		ASTNode* identifier(Symbol sym);
		ASTNode* break_() { return create(ASTNodeTypeBreak); }
		ASTNode* continue_() { return create(ASTNodeTypeContinue); }
		ASTNode* self() { return create(ASTNodeTypeSelf); }
		ASTNode* here() { return create(ASTNodeTypeHere); }
		ASTNode* it() { return create(ASTNodeTypeIt); }
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
		ASTNode* seq = create(ASTNodeTypeSequence);
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
		ASSERT(seq->type == ASTNodeTypeSequence);
		++seq->sequence.length;
		if (!seq->sequence.head) { seq->sequence.head = seq->sequence.tail = n; }
		else { seq->sequence.tail->next = n; seq->sequence.tail = n; }
	}
	
	inline ASTNode* AST::literal(const Immediate& val) {
		ASTNode* n = create(ASTNodeTypeLiteral);
		n->literal.value = new Immediate(val);
		return n;
	}
	
	inline ASTNode* AST::closure(ASTNode* parameters, ASTNode* body) {
		ASTNode* n = create(ASTNodeTypeClosure);
		n->closure.parameters = parameters; n->closure.body = body;
		return n;
	}
	
	inline ASTNode* AST::parameter(Symbol name, ASTNode* id_type, ASTNode* default_value) {
		ASTNode* n = create(ASTNodeTypeParameter);
		n->parameter.name = name;
		n->parameter.id_type = id_type;
		n->parameter.default_value = default_value;
		return n;
	}
	
	inline ASTNode* AST::return_(ASTNode* value) {
		ASTNode* n = create(ASTNodeTypeReturn);
		n->return_expr.value = value;
		return n;
	}
	
	inline ASTNode* AST::identifier(Symbol sym) {
		ASTNode* n = create(ASTNodeTypeIdentifier);
		n->identifier.name = sym;
		return n;
	}
	
	inline ASTNode* AST::assign(ASTNode* target, ASTNode* value) {
		ASTNode* n = create(ASTNodeTypeAssign);
		n->assign.target = target;
		n->assign.value = value;
		return n;
	}
	
	inline ASTNode* AST::method(ASTNode* object, Symbol sym) {
		ASTNode* n = create(ASTNodeTypeMethod);
		n->method.object = object;
		n->method.name = sym;
		return n;
	}
	
	inline ASTNode* AST::instance_variable(ASTNode* object, Symbol sym) {
		ASTNode* n = create(ASTNodeTypeInstanceVariable);
		n->instance_variable.object = object;
		n->instance_variable.name = sym;
		return n;
	}
	
	inline ASTNode* AST::call(ASTNode* object, ASTNode* arguments) {
		ASTNode* n = create(ASTNodeTypeCall);
		n->call.object = object;
		n->call.args = arguments;
		return n;
	}
	
	inline ASTNode* AST::association(ASTNode* object, ASTNode* arguments) {
		ASTNode* n = create(ASTNodeTypeAssociation);
		n->association.object = object;
		n->association.args = arguments;
		return n;
	}
	
	inline ASTNode* AST::named_argument(Symbol name, ASTNode* expr) {
		ASTNode* n = create(ASTNodeTypeNamedArgument);
		n->named_argument.name = name;
		n->named_argument.expr = expr;
		return n;
	}
	
	inline ASTNode* AST::logic_and(ASTNode* left, ASTNode* right) {
		ASTNode* n = create(ASTNodeTypeAnd);
		n->logic_and.left = left;
		n->logic_and.right = right;
		return n;
	}
	
	inline ASTNode* AST::logic_or(ASTNode* left, ASTNode* right) {
		ASTNode* n = logic_and(left, right); n->type = ASTNodeTypeOr;
		return n;
	}
	
	inline ASTNode* AST::logic_xor(ASTNode* left, ASTNode* right) {
		ASTNode* n = logic_and(left, right); n->type = ASTNodeTypeXor;
		return n;
	}
	
	inline ASTNode* AST::logic_not(ASTNode* expr) {
		ASTNode* n = create(ASTNodeTypeNot);
		n->logic_not.expr = expr;
		return n;
	}
	
	inline ASTNode* AST::loop(ASTNode* cond, ASTNode* body) {
		ASTNode* n = create(ASTNodeTypeLoop);
		n->loop.cond = cond;
		n->loop.body = body;
		return n;
	}
	
	inline ASTNode* AST::if_else(ASTNode* cond, ASTNode* body, ASTNode* else_body) {
		ASTNode* n = create(ASTNodeTypeIfElse);
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
