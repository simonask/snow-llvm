#include "codegen.hpp"
#include "asm.hpp"
#include "../util.hpp"

#include "snow/snow.h"
#include "snow/array.h"
#include "snow/class.h"

#include <map>
#include <algorithm>

#define CALL(FUNC) call_direct((void(*)(void))FUNC)

namespace {
	using namespace snow;
	
	struct VariableReference : public SnVariableReference {
		bool operator==(const VariableReference& other) const { return level == other.level && index == other.index; }
	};
	
	struct Alloca {
		Alloca(Asm& a, const snow::Operand& target, size_t num_elements, size_t element_size = 1)
			: a(a), size(num_elements*element_size)
		{
			if (size) {
				// 16-byte-align
				size_t pad = size % 16;
				if (pad) size += 16 - pad;
				a.subq(size, RSP);
				a.movq(RSP, target);
			}
		}
		
		~Alloca() {
			if (size) {
				a.addq(size, RSP);
			}
		}
		
		snow::Asm& a;
		size_t size;
	};
	
	struct Temporary {
		Temporary(Codegen::Function& f);
		~Temporary();
		operator Operand() const;
		
		Codegen::Function& f;
		int idx;
	};
	
	struct TemporaryArray {
		TemporaryArray(Codegen::Function& func, size_t size) : size(size) {
			elements = new Element[size];
			construct_range(reinterpret_cast<Temporary*>(elements), size, func);
		}
		~TemporaryArray() {
			destruct_range(reinterpret_cast<Temporary*>(elements), size);
			delete[] elements;
		}
		Temporary& operator[](size_t idx) { return *(reinterpret_cast<Temporary*>(elements + idx)); }
		
		size_t size;
	private:
		struct Element { byte _[sizeof(Temporary)]; };
		Element* elements;
	};
}

namespace snow {
	class Codegen::Function : public Asm {
	public:
		typedef std::vector<SnSymbol> Names;
		
		Function(Codegen& codegen) :
			codegen(codegen),
			materialized_descriptor_offset(0),
			materialized_code_offset(0),
			parent(NULL),
			name(0),
			it_index(0),
			needs_environment(true),
			num_temporaries(0)
		{}

		Codegen& codegen;
		size_t materialized_descriptor_offset;
		size_t materialized_code_offset;
		Function* parent;
		SnSymbol name;
		Names local_names;
		Names param_names;
		int it_index;
		bool needs_environment;
		
		int32_t stack_size_offset;
		Label* return_label;
		struct DirectCall { void* callee; size_t offset; };
		std::vector<DirectCall> direct_calls;
		struct FunctionDescriptorReference { Function* function; size_t offset; };
		std::vector<FunctionDescriptorReference> function_descriptor_references;
		std::vector<size_t> gc_root_offsets;
		struct GlobalReference { int32_t global_idx; size_t offset; };
		std::vector<GlobalReference> global_references;

		int num_temporaries;
		std::vector<int> temporaries_freelist;
		int alloc_temporary();
		void free_temporary(int);
		Operand temporary(int);
		
		bool compile_function_body(const SnAstNode* body_seq);
		void materialize_at(byte* destination);
	private:
		bool compile_ast_node(const SnAstNode* node);
		bool compile_assignment(const SnAstNode* assign);
		bool compile_call(const SnAstNode* call);
		void compile_call(const Operand& functor, const Operand& self, size_t num_args, const Operand& args_ptr, size_t num_names = 0, const Operand& names_ptr = Operand());
		void compile_method_call(const Operand& self, SnSymbol method_name, size_t num_args, const Operand& args_ptr, size_t num_names = 0, const Operand& names_ptr = Operand());
		void compile_get_method_or_property_inline_cache(const Operand& self, SnSymbol name, const Register& out_type, const Register& out_method_or_property);
		void compile_get_index_of_field_inline_cache(const Operand& self, SnSymbol name, const Register& target);
		Operand compile_get_address_for_local(const Register& reg, SnSymbol name, bool can_define = false);
		Function* compile_function(const SnAstNode* function);
		void call_direct(void(*callee)(void));
		void call_indirect(void(*callee)(void));
		
		void compile_alloca(size_t num_bytes, const Operand& out_ptr);
	};
	
	inline int Codegen::Function::alloc_temporary() {
		if (temporaries_freelist.size()) {
			int tmp = temporaries_freelist.back();
			temporaries_freelist.pop_back();
			return tmp;
		}
		return num_temporaries++;
	}
	
	inline void Codegen::Function::free_temporary(int idx) {
		temporaries_freelist.push_back(idx);
	}
	
	inline Operand Codegen::Function::temporary(int idx) {
		return address(RBP, -(idx+1)*sizeof(VALUE));
	}
	
	#define REG_CALL_FRAME R12
	#define REG_SELF       R13
	#define REG_IT         R14
	#define REG_FUNCTION   R15
	
	bool Codegen::Function::compile_function_body(const SnAstNode* body_seq) {
		pushq(RBP);
		movq(RSP, RBP);
		size_t stack_size_offset = subq(0, RSP);
		pushq(RBX);
		pushq(REG_CALL_FRAME);
		pushq(REG_SELF);
		pushq(REG_IT);
		pushq(REG_FUNCTION);
		movq(RDI, REG_FUNCTION);
		movq(RSI, REG_CALL_FRAME);
		movq(RDX, REG_SELF);
		movq(RCX, REG_IT);
		xorq(RAX, RAX); // always clear RAX, so empty functions return nil.
		return_label = label();
		
		if (!compile_ast_node(body_seq)) return false;
		
		bind_label(return_label);
		popq(REG_FUNCTION);
		popq(REG_IT);
		popq(REG_SELF);
		popq(REG_CALL_FRAME);
		popq(RBX);
		leave();
		ret();
		
		// set stack size
		int stack_size = (num_temporaries) * sizeof(VALUE);
		size_t pad = stack_size % 16;
		if (pad != 8) stack_size += 8;
		for (size_t i = 0; i < 4; ++i) {
			code()[stack_size_offset+i] = reinterpret_cast<byte*>(&stack_size)[i];
		}
		
		return true;
	}
	
	bool Codegen::Function::compile_ast_node(const SnAstNode* node) {
		switch (node->type) {
			case SN_AST_SEQUENCE: {
				for (const SnAstNode* x = node->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
				}
				return true;
			}
			case SN_AST_LITERAL: {
				uint64_t imm = (uint64_t)node->literal.value;
				movq(imm, RAX);
				return true;
			}
			case SN_AST_CLOSURE: {
				Function* f = compile_function(node);
				if (!f) return false;
				FunctionDescriptorReference ref;
				ref.offset = movq(0, RDI);
				ref.function = f;
				function_descriptor_references.push_back(ref);
				movq(REG_CALL_FRAME, RSI);
				CALL(snow_create_function);
				return true;
			}
			case SN_AST_RETURN: {
				if (node->return_expr.value) {
					if (!compile_ast_node(node->return_expr.value)) return false;
				}
				jmp(return_label);
				return true;
			}
			case SN_AST_IDENTIFIER: {
				Operand local_addr = compile_get_address_for_local(RDX, node->identifier.name, false);
				if (local_addr.is_valid()) {
					movq(local_addr, RAX);
				} else {
					movq(REG_CALL_FRAME, RDI);
					movq(node->identifier.name, RSI);
					CALL(snow_local_missing);
				}
				return true;
			}
			case SN_AST_SELF: {
				movq(REG_SELF, RAX);
				return true;
			}
			case SN_AST_HERE: {
				movq(REG_CALL_FRAME, RAX);
				return true;
			}
			case SN_AST_IT: {
				movq(REG_IT, RAX);
				return true;
			}
			case SN_AST_ASSIGN: {
				return compile_assignment(node);
			}
			case SN_AST_METHOD: {
				if (!compile_ast_node(node->method.object)) return false;
				Temporary self(*this);
				movq(RAX, self);
				compile_get_method_or_property_inline_cache(RAX, node->method.name, RAX, RDI);
				Label* get_method = label();
				Label* after = label();
				
				cmpb(SnMethodTypeFunction, RAX);
				j(CC_EQUAL, get_method);
				
				{
					// get property
					xorq(RCX, RCX);
					compile_call(RDI, self, 0, RCX);
					jmp(after);
				}
				
				{
					// get method proxy wrapper
					bind_label(get_method);
					movq(RDI, RSI);
					movq(self, RDI);
					CALL(snow_create_method_proxy);
					// jmp(after);
				}
				
				bind_label(after);
				return true;
			}
			case SN_AST_INSTANCE_VARIABLE: {
				if (!compile_ast_node(node->method.object)) return false;
				Temporary self(*this);
				movq(RAX, self);
				compile_get_index_of_field_inline_cache(RAX, node->instance_variable.name, RSI);
				movq(self, RDI);
				CALL(snow_object_get_field_by_index);
				return true;
			}
			case SN_AST_CALL: {
				return compile_call(node);
			}
			case SN_AST_ASSOCIATION: {
				if (!compile_ast_node(node->association.object)) return false;
				Temporary self(*this);
				movq(RAX, self);
				
				size_t num_args = node->association.args->sequence.length;
				Temporary args_ptr(*this);
				Alloca _1(*this, args_ptr, sizeof(VALUE)*num_args);
				
				size_t i = 0;
				for (SnAstNode* x = node->association.args->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
					movq(args_ptr, RCX);
					movq(RAX, address(RCX, sizeof(VALUE) * i++));
				}
				compile_method_call(self, snow_sym("get"), num_args, args_ptr);
				return true;
			}
			case SN_AST_AND: {
				Label* left_true = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_and.left)) return false;
				movq(RAX, RBX);
				movq(RAX, RDI);
				CALL(snow_eval_truth);
				cmpq(0, RAX);
				j(CC_NOT_EQUAL, left_true);
				movq(RBX, RAX);
				jmp(after);
				
				bind_label(left_true);
				if (!compile_ast_node(node->logic_and.right)) return false;

				bind_label(after);
				return true;
			}
			case SN_AST_OR: {
				Label* left_false = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_or.left)) return false;
				movq(RAX, RBX);
				movq(RAX, RDI);
				CALL(snow_eval_truth);
				cmpq(0, RAX);
				j(CC_EQUAL, left_false);
				movq(RBX, RAX);
				jmp(after);
				
				bind_label(left_false);
				if (!compile_ast_node(node->logic_or.right)) return false;

				bind_label(after);
				return true;
			}
			case SN_AST_XOR: {
				Label* equal_truth = label();
				Label* after = label();
				Label* left_is_true = label();
				
				if (!compile_ast_node(node->logic_xor.left)) return false;
				Temporary left_value(*this);
				movq(RAX, left_value); // save left value
				
				if (!compile_ast_node(node->logic_xor.right)) return false;
				movq(RAX, RBX); // save right value in caller-preserved register
				movq(RAX, RDI);
				CALL(snow_eval_truth);
				Temporary right_truth(*this);
				movq(RAX, right_truth);
				movq(left_value, RDI);
				CALL(snow_eval_truth);
				cmpb(RAX, right_truth); // compare left truth to right truth
				j(CC_EQUAL, equal_truth);
				cmpq(0, RAX); // compare left truth to zero
				j(CC_NOT_EQUAL, left_is_true);
				movq(RBX, RAX); // previously saved right value
				jmp(after);
				
				bind_label(left_is_true);
				movq(left_value, RAX);
				jmp(after);
				
				bind_label(equal_truth);
				movq((uint64_t)SN_FALSE, RAX);
				
				bind_label(after);
				return true;
			}
			case SN_AST_NOT: {
				Label* truth = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_not.expr)) return false;
				cmpq(0, RAX);
				j(CC_NOT_EQUAL, truth);
				movq((uint64_t)SN_FALSE, RAX);
				jmp(after);
				
				bind_label(truth);
				movq((uint64_t)SN_TRUE, RAX);
				
				bind_label(after);
				return true;
			}
			case SN_AST_LOOP: {
				Label* cond = label();
				Label* after = label();
				
				bind_label(cond);
				if (!compile_ast_node(node->loop.cond)) return false;
				movq(RAX, RDI);
				CALL(snow_eval_truth);
				cmpq(0, RAX);
				j(CC_EQUAL, after);
				
				if (!compile_ast_node(node->loop.body)) return false;
				jmp(cond);
				
				bind_label(after);
				return true;
			}
			case SN_AST_BREAK: {
				// TODO!
				return true;
			}
			case SN_AST_CONTINUE: {
				// TODO!
				return true;
			}
			case SN_AST_IF_ELSE: {
				Label* else_body = label();
				Label* after = label();
				
				if (!compile_ast_node(node->if_else.cond)) return false;
				movq(RAX, RDI);
				CALL(snow_eval_truth);
				cmpq(0, RAX);
				j(CC_EQUAL, else_body);
				if (!compile_ast_node(node->if_else.body)) return false;
				
				if (node->if_else.else_body) {
					jmp(after);
					bind_label(else_body);
					if (!compile_ast_node(node->if_else.else_body)) return false;
				} else {
					bind_label(else_body);
				}
				bind_label(after);
				return true;
			}
			default: {
				fprintf(stderr, "CODEGEN: Inappropriate AST node in tree (type %d).\n", node->type);
				return false;
			}
		}
	}
	
	bool Codegen::Function::compile_assignment(const SnAstNode* node) {
		ASSERT(node->assign.value->type == SN_AST_SEQUENCE);
		ASSERT(node->assign.target->type == SN_AST_SEQUENCE);
		
		// gather assign targets
		const size_t num_targets = node->assign.target->sequence.length;
		SnAstNode* targets[num_targets];
		size_t i = 0;
		for (SnAstNode* x = node->assign.target->sequence.head; x; x = x->next) {
			targets[i++] = x;
		}
		
		// allocate temporaries for this assignment
		const size_t num_values = node->assign.value->sequence.length;
		TemporaryArray values(*this, num_values);
		// compile assignment values
		i = 0;
		for (SnAstNode* x = node->assign.value->sequence.head; x; x = x->next) {
			// TODO: Assignment name
			if (!compile_ast_node(x)) return false;
			movq(RAX, values[i++]);
		}
		
		// assign values to targets
		for (size_t i = 0; i < num_targets; ++i) {
			// If this is the last target, and there are several remaining
			// values, put the rest of them in an array, and assign that.
			if (i == num_targets-1 && num_values > num_targets) {
				size_t num_remaining = num_values - num_targets + 1;
				movq(num_remaining, RDI);
				CALL(snow_create_array_with_size);
				movq(RAX, RBX);
				for (size_t j = i; j < num_values; ++j) {
					movq(RBX, RDI);
					movq(values[j], RSI);
					CALL(snow_array_push);
				}
				movq(RBX, values[i]);
			}
			
			SnAstNode* target = targets[i];
			switch (target->type) {
				case SN_AST_ASSOCIATION: {
					size_t num_args = target->association.args->sequence.length + 1;
					Temporary args_ptr(*this);
					Alloca _1(*this, args_ptr, sizeof(VALUE) * num_args);
					
					size_t j = 0;
					for (SnAstNode* x = target->association.args->sequence.head; x; x = x->next) {
						if (!compile_ast_node(x)) return false;
						movq(args_ptr, RCX);
						movq(RAX, address(RCX, sizeof(VALUE) * j++));
					}
					if (!j) movq(args_ptr, RCX); // if j != 0, args_ptr is already in RCX from above
					if (i < num_values)
						movq(values[i], RDX);
					else
						movq((uint64_t)SN_NIL, RDX);
					movq(RDX, address(RCX, sizeof(VALUE) * j));
					
					if (!compile_ast_node(target->association.object)) return false;
					compile_method_call(RAX, snow_sym("set"), num_args, args_ptr);
					break;
				}
				case SN_AST_INSTANCE_VARIABLE: {
					if (!compile_ast_node(target->instance_variable.object)) return false;
					Temporary obj(*this);
					movq(RAX, obj);
					compile_get_index_of_field_inline_cache(RAX, target->instance_variable.name, RSI);
					movq(obj, RDI);
					if (i <= num_values)
						movq(values[i], RDX);
					else
						xorq(RDX, RDX);
					CALL(snow_object_set_field_by_index);
					break;
				}
				case SN_AST_IDENTIFIER: {
					Operand local_addr = compile_get_address_for_local(RDX, target->identifier.name, true);
					ASSERT(local_addr.is_valid());
					movq(values[i], RAX);
					movq(RAX, local_addr);
					break;
				}
				case SN_AST_METHOD: {
					if (!compile_ast_node(target->method.object)) return false;
					movq(RAX, RDI);
					movq(target->method.name, RSI);
					if (i <= num_values)
						movq(values[i], RDX);
					else
						xorq(RDX, RDX);
					CALL(snow_object_set_property_or_define_method);
				}
				default:
					fprintf(stderr, "CODEGEN: Invalid target for assignment! (type=%d)", target->type);
					return false;
			}
		}
		return true;
	}
	
	Codegen::Function* Codegen::Function::compile_function(const SnAstNode* function) {
		ASSERT(function->type == SN_AST_CLOSURE);
		Function* f = new Function(codegen);
		if (function->closure.parameters) {
			f->param_names.reserve(function->closure.parameters->sequence.length);
			for (const SnAstNode* x = function->closure.parameters->sequence.head; x; x = x->next) {
				ASSERT(x->type == SN_AST_PARAMETER);
				f->param_names.push_back(x->parameter.name);
				f->local_names.push_back(x->parameter.name);
			}
		}
		if (!f->compile_function_body(function->closure.body)) {
			delete f;
			return NULL;
		}
		codegen._functions.push_back(f);
		return f;
	}
	
	Operand Codegen::Function::compile_get_address_for_local(const Register& reg, SnSymbol name, bool can_define) {
		// Look for locals
		Function* f = this;
		ssize_t level = 0;
		ssize_t index = -1;
		while (f) {
			index = index_of(f->local_names, name);
			if (index >= 0) break;
			f = f->parent;
			++level;
		}
		if (index >= 0) {
			// local found!
			movq(REG_CALL_FRAME, reg); // 'here'
			for (ssize_t i = 0; i < level; ++i) {
				// TODO: It would be nice not to have to go through this whole
				// chain whenever we want to get the address of a local in a parent scope.
				// Oh well, at least there's no calls.
				// TODO: Variable references
				movq(address(reg, offsetof(SnCallFrame, function)), reg);
				movq(address(reg, offsetof(SnFunction, definition_context)), reg);
			}
			movq(address(reg, offsetof(SnCallFrame, locals)), reg);
			return address(reg, index * sizeof(VALUE));
		}
		
		// Look for module globals.
		ssize_t global_idx = index_of(codegen.module_globals, name);
		if (global_idx >= 0) {
			return global(global_idx);
		}
		
		if (can_define) {
			if (parent) {
				// if we're not in module global scope, define a regular local
				index = local_names.size();
				local_names.push_back(name);
				movq(REG_CALL_FRAME, reg); // 'here'
				movq(address(reg, offsetof(SnCallFrame, locals)), reg);
				return address(reg, index * sizeof(VALUE));
			} else {
				global_idx = codegen.module_globals.size();
				codegen.module_globals.push_back(name);
				return global(global_idx);
			}
		} else {
			return Operand(); // Invalid operand.
		}
	}
	
	struct CompareNamedArguments {
		int operator()(const SnAstNode* a, const SnAstNode* b) {
			ASSERT(a->type == SN_AST_NAMED_ARGUMENT);
			ASSERT(b->type == SN_AST_NAMED_ARGUMENT);
			return (int)((ssize_t)a->named_argument.name - (ssize_t)b->named_argument.name);
		}
	};
	
	bool Codegen::Function::compile_call(const SnAstNode* node) {
		size_t num_arguments = 0;
		size_t unnamed_arguments_offset = 0;
		std::map<const SnAstNode*, int> sorted_argument_order;
		std::vector<const SnAstNode*> named_args;
		
		if (node->call.args) {
			num_arguments = node->call.args->sequence.length;
		}
		
		const SnAstNode* all_args[num_arguments];
			
		if (num_arguments) {
			size_t i = 0;
			for (const SnAstNode* x = node->call.args->sequence.head; x; x = x->next) {
				if (x->type == SN_AST_NAMED_ARGUMENT) {
					named_args.push_back(x);
					++unnamed_arguments_offset;
				}
				all_args[i++] = x;
			}
			
			std::sort(named_args.begin(), named_args.end(), CompareNamedArguments());
			for (int i = 0; i < named_args.size(); ++i) {
				sorted_argument_order[named_args[i]] = i;
			}
		}
		
		Temporary args_ptr(*this);
		Temporary names_ptr(*this);
		Alloca _1(*this, args_ptr, num_arguments, sizeof(VALUE));
		Alloca _2(*this, names_ptr, named_args.size(), sizeof(SnSymbol));
		
		for (size_t i = 0; i < num_arguments; ++i) {
			const SnAstNode* x = all_args[i];
			size_t position;
			if (x->type == SN_AST_NAMED_ARGUMENT) {
				if (!compile_ast_node(x->named_argument.expr)) return false;
				position = sorted_argument_order[x];
				movq(names_ptr, RDX);
				movq(x->named_argument.name, RDI);
				movq(RDI, address(RDX, sizeof(SnSymbol) * position));
			} else {
				if (!compile_ast_node(x)) return false;
				position = unnamed_arguments_offset++;
			}
			movq(args_ptr, RDX);
			movq(RAX, address(RDX, sizeof(VALUE) * position));
		}
		
		if (node->call.object->type == SN_AST_METHOD) {
			if (!compile_ast_node(node->call.object->method.object)) return false;
			compile_method_call(RAX, node->call.object->method.name, num_arguments, args_ptr, named_args.size(), names_ptr);
		} else {
			if (!compile_ast_node(node->call.object)) return false;
			xorq(RSI, RSI); // self = NULL
			compile_call(RAX, RSI, num_arguments, args_ptr, named_args.size(), names_ptr);
		}
		return true;
	}
	
	void Codegen::Function::compile_call(const Operand& functor, const Operand& self, size_t num_args, const Operand& args_ptr, size_t num_names, const Operand& names_ptr) {
		movq(functor, RDI);
		movq(self, RSI);
		if (num_names) {
			ASSERT(num_args >= num_names);
			movq(num_names, RDX);
			movq(names_ptr, RCX);
			movq(num_args, R8);
			movq(args_ptr, R9);
			CALL(snow_call_with_named_arguments);
		} else {
			if (num_args)
				movq(num_args, RDX);
			else
				xorq(RDX, RDX);
			movq(args_ptr, RCX);
			CALL(snow_call);
		}
	}
	
	void Codegen::Function::compile_method_call(const Operand& in_self, SnSymbol method_name, size_t num_args, const Operand& args_ptr, size_t num_names, const Operand& names_ptr) {
		ASSERT(args_ptr.is_memory());
		if (num_names) ASSERT(names_ptr.is_memory());
		Temporary self(*this);
		movq(in_self, self);
		
		Label* property_call = label();
		Label* after = label();
		
		compile_get_method_or_property_inline_cache(in_self, method_name, RSI, RDI);
		cmpl(SnMethodTypeFunction, RSI);
		j(CC_NOT_EQUAL, property_call);
		
		{
			// Method call
			compile_call(RDI, self, num_args, args_ptr, num_names, names_ptr);
			jmp(after);
		}
		
		{
			// Property call
			bind_label(property_call);
			xorq(RCX, RCX);
			compile_call(RDI, self, 0, RCX);
			compile_call(RAX, self, num_args, args_ptr, num_names, names_ptr);
			// jmp(after);
		}
		
		bind_label(after);
	}
	
	void Codegen::Function::compile_get_method_or_property_inline_cache(const Operand& object, SnSymbol name, const Register& out_type, const Register& out_method_or_property_getter) {
		Label* uninitialized = label();
		Label* monomorphic = label();
		Label* miss = label();
		Label* after = label();
		
		// TODO: Make this work for W^X systems.
		
		// Data labels
		Label* state_jmp_data = label();
		Label* monomorphic_class_data = label();
		Label* monomorphic_method_type_data = label();
		Label* monomorphic_method_data = label();
		
		movq(object, RDI);
		CALL(snow_get_class);
		bind_label_at(state_jmp_data, jmp(uninitialized));
		
		{
			bind_label(uninitialized);
			Alloca _1(*this, RBX, sizeof(SnClass*) + sizeof(SnMethod), 1);
			movq(RAX, address(RBX));
			movq(RAX, RDI);
			movq(name, RSI);
			leaq(address(RBX, sizeof(SnClass*)), RDX);
			CALL(snow_class_get_method_or_property);
			movl_label_to_jmp(monomorphic, state_jmp_data);
			movq(address(RBX), RAX);
			movq(RAX, Operand(monomorphic_class_data));
			movq(address(RBX, sizeof(SnClass*) + offsetof(SnMethod, type)), out_type);
			movq(out_type, Operand(monomorphic_method_type_data));
			movq(address(RBX, sizeof(SnClass*) + offsetof(SnMethod, function)), out_method_or_property_getter);
			movq(out_method_or_property_getter, Operand(monomorphic_method_data));
			jmp(after);
		}
		
		{
			bind_label(monomorphic);
			// compare incoming class (in RAX from call to snow_get_class) with cached value
			bind_label_at(monomorphic_class_data, movq(0, RDI));
			cmpq(RDI, RAX); // cmpq does not support 8-byte immediates
			gc_root_offsets.push_back(monomorphic_class_data->offset);
			j(CC_NOT_EQUAL, miss);
			bind_label_at(monomorphic_method_type_data, movq(0, out_type));
			bind_label_at(monomorphic_method_data, movq(0, out_method_or_property_getter));
			gc_root_offsets.push_back(monomorphic_method_data->offset);
			jmp(after);
		}
		
		{
			bind_label(miss);
			Alloca _1(*this, RBX, sizeof(SnMethod), 1);
			movq(RBX, RDX);
			movq(RAX, RDI);
			movq(name, RSI);
			CALL(snow_class_get_method_or_property);
			movq(address(RBX, offsetof(SnMethod, type)), out_type);
			movq(address(RBX, offsetof(SnMethod, function)), out_method_or_property_getter);
		}
		
		bind_label(after);
	}
	
	void Codegen::Function::compile_get_index_of_field_inline_cache(const Operand& object, SnSymbol name, const Register& target) {
		Label* uninitialized = label();
		Label* monomorphic = label();
		Label* miss = label();
		Label* after = label();
		
		// TODO: Make this work for W^X systems.
		
		Label* state_jmp_data = label();
		Label* monomorphic_class_data = label();
		Label* monomorphic_iv_offset_data = label();
		
		movq(object, RDI);
		CALL(snow_get_class);
		bind_label_at(state_jmp_data, jmp(uninitialized));
		
		{
			bind_label(uninitialized);
			Temporary in_class(*this);
			movq(RAX, in_class);
			movq(RAX, RDI);
			movq(name, RSI);
			CALL(snow_class_get_index_of_field);
			movq(RAX, target);
			cmpq(0, target);
			j(CC_LESS, after);
			// change state to monomorphic
			movl_label_to_jmp(monomorphic, state_jmp_data); // XXX: Modifying code
			movq(in_class, RDI);
			movq(RDI, Operand(monomorphic_class_data));
			movl(target, Operand(monomorphic_iv_offset_data)); // XXX: Modifying code
			jmp(after);
		}
		
		{
			bind_label(monomorphic);
			bind_label_at(monomorphic_class_data, movq(0, RDI));
			gc_root_offsets.push_back(monomorphic_class_data->offset);
			cmpq(RDI, RAX);
			j(CC_NOT_EQUAL, miss);
			bind_label_at(monomorphic_iv_offset_data, movl(0, target));
			jmp(after);
		}

		{
			bind_label(miss);
			movq(RAX, RDI);
			movq(name, RSI);
			CALL(snow_class_get_index_of_field);
			movq(RAX, target);
		}
		
		bind_label(after);
	}
	
	void Codegen::Function::call_direct(void(*callee)(void)) {
		DirectCall dc;
		dc.offset = call(0);
		dc.callee = (void*)callee;
		direct_calls.push_back(dc);
	}
	
	void Codegen::Function::call_indirect(void(*callee)(void)) {
		movq((uint64_t)callee, R10);
		call(R10);
	}
	
	void Codegen::Function::materialize_at(byte* destination) {
		link_labels();
		
		for (size_t i = 0; i < code().size(); ++i) {
			destination[i] = code()[i];
		}
	}
	
	
	bool Codegen::compile_ast(const SnAST* ast) {
		Function* function = new Function(*this);
		_functions.push_back(function);
		_entry = function;
		return function->compile_function_body(ast->_root);
	}
	
	size_t Codegen::compiled_size() const {
		size_t accum = module_globals.size() * sizeof(VALUE);
		for (size_t i = 0; i < _functions.size(); ++i) {
			accum += sizeof(SnFunctionDescriptor);
			accum += _functions[i]->code().size();
		}
		return accum;
	}
	
	size_t Codegen::materialize_at(byte* destination) {
		size_t offset = 0;
		
		// materialize globals
		memset(destination, 0, module_globals.size() * sizeof(VALUE));
		offset += module_globals.size() * sizeof(VALUE);
		
		// materialize function descriptors
		for (size_t i = 0; i < _functions.size(); ++i) {
			Function* f = _functions[i];
			f->materialized_descriptor_offset = offset;
			
			SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)(destination + offset);
			offset += sizeof(SnFunctionDescriptor);
			
			descriptor->ptr = 0; // linked later
			descriptor->name = f->name;
			descriptor->return_type = SnAnyType;
			size_t num_params = f->param_names.size();
			descriptor->num_params = num_params;
			descriptor->param_types = (SnType*)(destination + offset);
			offset += sizeof(SnType) * num_params;
			descriptor->param_names = (SnSymbol*)(destination + offset);
			offset += sizeof(SnSymbol) * num_params;
			for (size_t j = 0; j < num_params; ++j) {
				descriptor->param_types[j] = SnAnyType; // TODO
				descriptor->param_names[j] = f->param_names[j];
			}
			
			descriptor->it_index = f->it_index;
			
			size_t num_locals = f->local_names.size();
			descriptor->num_locals = num_locals;
			descriptor->local_names = (SnSymbol*)(destination + offset);
			offset += sizeof(SnSymbol) * num_locals;
			for (size_t j = 0; j < num_locals; ++j) {
				descriptor->local_names[j] = f->local_names[j];
			}
			
			descriptor->needs_context = f->needs_environment;
			descriptor->jit_info = NULL;
			descriptor->num_variable_references = 0; // TODO
			descriptor->variable_references = NULL;
		}
		
		std::vector<Function::FunctionDescriptorReference> descriptor_references;
		std::vector<Function::DirectCall> direct_calls;
		
		// materialize code
		for (size_t i = 0; i < _functions.size(); ++i) {
			Function* f = _functions[i];
			f->link_globals(offset);
			f->materialize_at(destination + offset);
			f->materialized_code_offset = offset;
			SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)(destination + f->materialized_descriptor_offset);
			descriptor->ptr = (SnFunctionPtr)(destination + offset);
			
			// collect descriptor references
			for (size_t j = 0; j < f->function_descriptor_references.size(); ++j) {
				Function::FunctionDescriptorReference ref;
				ref.function = f->function_descriptor_references[j].function;
				ref.offset = offset + f->function_descriptor_references[j].offset;
				descriptor_references.push_back(ref);
			}
			
			// collect direct calls
			for (size_t j = 0; j < f->direct_calls.size(); ++j) {
				direct_calls.push_back(f->direct_calls[j]);
				direct_calls.back().offset += offset;
			}
			
			offset += f->code().size();
		}
		
		// link descriptor references
		for (size_t i = 0; i < descriptor_references.size(); ++i) {
			uint64_t* site = (uint64_t*)(destination + descriptor_references[i].offset);
			*site = (uint64_t)(destination + descriptor_references[i].function->materialized_descriptor_offset);
		}
		
		// link direct calls
		for (size_t i = 0; i < direct_calls.size(); ++i) {
			size_t o = direct_calls[i].offset;
			byte* callee = (byte*)direct_calls[i].callee;
			static const size_t direct_call_instr_len = 4;
			intptr_t diff = callee - (destination + o + direct_call_instr_len);
			ASSERT(diff <= INT_MAX && diff >= INT_MIN);
			for (size_t j = 0; j < 4; ++j) {
				destination[o + j] = reinterpret_cast<byte*>(&diff)[j];
			}
		}
		
		return offset;
	}
	
	size_t Codegen::get_offset_for_entry_descriptor() const {
		return _entry->materialized_descriptor_offset;
	}
}

namespace {
	using namespace snow;
	Temporary::Temporary(Codegen::Function& f) : f(f) {
		idx = f.alloc_temporary();
	}
	Temporary::~Temporary() {
		f.free_temporary(idx);
	}
	Temporary::operator Operand() const {
		return f.temporary(idx);
	}
}