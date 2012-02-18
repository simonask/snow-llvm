#pragma once
#ifndef AST_INTERN_HPP_UNJFFI95
#define AST_INTERN_HPP_UNJFFI95

#include "snow/ast.hpp"
#include "linkheap.hpp"
#include "lexer.hpp"

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
		ASTNode* literal(const LexerLocation& loc, Value val);
		ASTNode* closure(const LexerLocation& loc, ASTNode* parameters, ASTNode* body);
		ASTNode* parameter(const LexerLocation& loc, Symbol name, ASTNode* id_type, ASTNode* default_value);
		ASTNode* return_(const LexerLocation& loc, ASTNode* value = NULL);
		ASTNode* identifier(const LexerLocation& loc, Symbol sym);
		ASTNode* break_(const LexerLocation& loc) { return create(loc, ASTNodeTypeBreak); }
		ASTNode* continue_(const LexerLocation& loc) { return create(loc, ASTNodeTypeContinue); }
		ASTNode* self(const LexerLocation& loc) { return create(loc, ASTNodeTypeSelf); }
		ASTNode* here(const LexerLocation& loc) { return create(loc, ASTNodeTypeHere); }
		ASTNode* it(const LexerLocation& loc) { return create(loc, ASTNodeTypeIt); }
		ASTNode* assign(const LexerLocation& loc, ASTNode* target, ASTNode* value);
		ASTNode* method(const LexerLocation& loc, ASTNode* object, Symbol sym);
		ASTNode* instance_variable(const LexerLocation& loc, ASTNode* object, Symbol sym);
		ASTNode* call(const LexerLocation& loc, ASTNode* object, ASTNode* arguments);
		ASTNode* association(const LexerLocation& loc, ASTNode* object, ASTNode* arguments);
		ASTNode* named_argument(const LexerLocation& loc, Symbol name, ASTNode* expr);
		ASTNode* logic_and(const LexerLocation& loc, ASTNode* left, ASTNode* right);
		ASTNode* logic_or(const LexerLocation& loc, ASTNode* left, ASTNode* right);
		ASTNode* logic_xor(const LexerLocation& loc, ASTNode* left, ASTNode* right);
		ASTNode* logic_not(const LexerLocation& loc, ASTNode* expr);
		ASTNode* loop(const LexerLocation& loc, ASTNode* cond, ASTNode* body);
		ASTNode* if_else(const LexerLocation& loc, ASTNode* cond, ASTNode* body, ASTNode* else_body);
	private:
		LinkHeap<ASTNode> _heap;

		ASTNode* create(const LexerLocation& loc, ASTNodeType type);
		void print_r(ASTNode* n, int indent) const;
	};

	inline ASTNode* AST::sequence(unsigned int n, ...) {
		ASTNode* seq = create(LexerLocation(), ASTNodeTypeSequence);
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
	
	inline ASTNode* AST::literal(const LexerLocation& loc, Value val) {
		ASTNode* n = create(loc, ASTNodeTypeLiteral);
		n->literal.value = val;
		return n;
	}
	
	inline ASTNode* AST::closure(const LexerLocation& loc, ASTNode* parameters, ASTNode* body) {
		ASTNode* n = create(loc, ASTNodeTypeClosure);
		n->closure.parameters = parameters; n->closure.body = body;
		return n;
	}
	
	inline ASTNode* AST::parameter(const LexerLocation& loc, Symbol name, ASTNode* id_type, ASTNode* default_value) {
		ASTNode* n = create(loc, ASTNodeTypeParameter);
		n->parameter.name = name;
		n->parameter.id_type = id_type;
		n->parameter.default_value = default_value;
		return n;
	}
	
	inline ASTNode* AST::return_(const LexerLocation& loc, ASTNode* value) {
		ASTNode* n = create(loc, ASTNodeTypeReturn);
		n->return_expr.value = value;
		return n;
	}
	
	inline ASTNode* AST::identifier(const LexerLocation& loc, Symbol sym) {
		ASTNode* n = create(loc, ASTNodeTypeIdentifier);
		n->identifier.name = sym;
		return n;
	}
	
	inline ASTNode* AST::assign(const LexerLocation& loc, ASTNode* target, ASTNode* value) {
		ASTNode* n = create(loc, ASTNodeTypeAssign);
		n->assign.target = target;
		n->assign.value = value;
		return n;
	}
	
	inline ASTNode* AST::method(const LexerLocation& loc, ASTNode* object, Symbol sym) {
		ASTNode* n = create(loc, ASTNodeTypeMethod);
		n->method.object = object;
		n->method.name = sym;
		return n;
	}
	
	inline ASTNode* AST::instance_variable(const LexerLocation& loc, ASTNode* object, Symbol sym) {
		ASTNode* n = create(loc, ASTNodeTypeInstanceVariable);
		n->instance_variable.object = object;
		n->instance_variable.name = sym;
		return n;
	}
	
	inline ASTNode* AST::call(const LexerLocation& loc, ASTNode* object, ASTNode* arguments) {
		ASTNode* n = create(loc, ASTNodeTypeCall);
		n->call.object = object;
		n->call.args = arguments;
		return n;
	}
	
	inline ASTNode* AST::association(const LexerLocation& loc, ASTNode* object, ASTNode* arguments) {
		ASTNode* n = create(loc, ASTNodeTypeAssociation);
		n->association.object = object;
		n->association.args = arguments;
		return n;
	}
	
	inline ASTNode* AST::named_argument(const LexerLocation& loc, Symbol name, ASTNode* expr) {
		ASTNode* n = create(loc, ASTNodeTypeNamedArgument);
		n->named_argument.name = name;
		n->named_argument.expr = expr;
		return n;
	}
	
	inline ASTNode* AST::logic_and(const LexerLocation& loc, ASTNode* left, ASTNode* right) {
		ASTNode* n = create(loc, ASTNodeTypeAnd);
		n->logic_and.left = left;
		n->logic_and.right = right;
		return n;
	}
	
	inline ASTNode* AST::logic_or(const LexerLocation& loc, ASTNode* left, ASTNode* right) {
		ASTNode* n = logic_and(loc, left, right); n->type = ASTNodeTypeOr;
		return n;
	}
	
	inline ASTNode* AST::logic_xor(const LexerLocation& loc, ASTNode* left, ASTNode* right) {
		ASTNode* n = logic_and(loc, left, right); n->type = ASTNodeTypeXor;
		return n;
	}
	
	inline ASTNode* AST::logic_not(const LexerLocation& loc, ASTNode* expr) {
		ASTNode* n = create(loc, ASTNodeTypeNot);
		n->logic_not.expr = expr;
		return n;
	}
	
	inline ASTNode* AST::loop(const LexerLocation& loc, ASTNode* cond, ASTNode* body) {
		ASTNode* n = create(loc, ASTNodeTypeLoop);
		n->loop.cond = cond;
		n->loop.body = body;
		return n;
	}
	
	inline ASTNode* AST::if_else(const LexerLocation& loc, ASTNode* cond, ASTNode* body, ASTNode* else_body) {
		ASTNode* n = create(loc, ASTNodeTypeIfElse);
		n->if_else.cond = cond;
		n->if_else.body = body;
		n->if_else.else_body = else_body;
		return n;
	}

	inline ASTNode* AST::create(const LexerLocation& loc, ASTNodeType type) {
		ASTNode* n = _heap.alloc();
		n->type = type;
		n->next = NULL;
		n->line = loc.line;
		n->column = loc.column;
		return n;
	}
	
	inline void AST::clear() {
		_heap.clear();
		_root = NULL;
	}
}

#endif /* end of include guard: AST_INTERN_HPP_UNJFFI95 */
