#include "codegen.hpp"
#include "asm.hpp"
#include "snow/util.hpp"
#include "../function-internal.hpp"
#include "../inline-cache.hpp"

#include "snow/snow.hpp"
#include "snow/array.hpp"
#include "snow/class.hpp"

#include <map>
#include <algorithm>

#define CALL(FUNC) call_direct((void(*)(void))FUNC)

#define UNSAFE_OFFSET_OF(TYPE, FIELD) (size_t)(&((const TYPE*)NULL)->FIELD)

namespace {
	using namespace snow;
	
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
		typedef Placeholder<Temporary> Element;
		Element* elements;
	};
}

namespace snow {
	namespace ccall {
		void class_lookup_method(VALUE cls, Symbol name, MethodQueryResult* out_method) {
			snow::class_lookup_method(cls, name, out_method);
		}
		
		VALUE call_method(VALUE method, VALUE self, size_t num_args, const VALUE* args, MethodType type, Symbol name) {
			switch (type) {
				case MethodTypeNone: {
					ASSERT(false);
					return NULL;
				}
				case MethodTypeFunction: {
					return snow::call(method, self, num_args, reinterpret_cast<const Value*>(args));
				}
				case MethodTypeProperty: {
					Value functor = snow::call(method, self, 0, NULL);
					return snow::call(functor, self, num_args, reinterpret_cast<const Value*>(args));
				}
				case MethodTypeMissing: {
					SN_STACK_ARRAY(Value, new_args, num_args+1);
					new_args[0] = symbol_to_value(name);
					snow::copy_range(new_args+1, args, num_args);
					return snow::call(method, self, num_args+1, new_args);
				}
			}
		}
		
		int32_t class_get_index_of_instance_variable(VALUE cls, Symbol name) {
			return snow::class_get_index_of_instance_variable(cls, name);
		}
		
		VALUE call_frame_environment(CallFrame* call_frame) {
			return snow::call_frame_environment(call_frame);
		}
		
		void array_push(VALUE array, VALUE value) {
			snow::array_push(array, value);
		}
		
		Object* create_function_for_descriptor(const FunctionDescriptor* descriptor, VALUE definition_environment) {
			return snow::create_function_for_descriptor(descriptor, definition_environment);
		}
		
		Object* get_class(VALUE val) {
			return snow::get_class(val);
		}
		
		VALUE call(VALUE functor, VALUE self, size_t num_args, const VALUE* args) {
			const Value* vargs = reinterpret_cast<const Value*>(args); // TODO: Consider this
			return snow::call(functor, self, num_args, vargs);
		}
		
		VALUE call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, Symbol* names, size_t num_args, VALUE* args) {
			const Value* vargs = reinterpret_cast<const Value*>(args); // TODO: Consider this
			return snow::call_with_named_arguments(functor, self, num_names, names, num_args, vargs);
		}
		
		VALUE local_missing(CallFrame* here, Symbol name) {
			return snow::local_missing(here, name);
		}
		
		Object* create_method_proxy(VALUE object, VALUE method) {
			return snow::create_method_proxy(object, method);
		}
		
		Object* create_array_with_size(uint32_t size) {
			return snow::create_array_with_size(size);
		}
		
		Value* call_frame_get_locals(const CallFrame* here) {
			return here->locals;
		}
		
		int32_t object_get_or_create_index_of_instance_variable(VALUE obj, Symbol name) {
			return snow::object_get_or_create_index_of_instance_variable(obj, name);
		}
		
		int32_t object_get_index_of_instance_variable(VALUE obj, Symbol name) {
			return snow::object_get_index_of_instance_variable(obj, name);
		}
		
		VALUE object_get_instance_variable_by_index(VALUE obj, int32_t idx) {
			return snow::object_get_instance_variable_by_index(obj, idx);
		}
		
		VALUE object_set_instance_variable_by_index(VALUE obj, int32_t idx, VALUE val) {
			return snow::object_set_instance_variable_by_index(obj, idx, val);
		}
		
		VALUE object_set_property_or_define_method(VALUE obj, Symbol name, VALUE val) {
			return snow::object_set_property_or_define_method(obj, name, val);
		}
		
		VALUE call_frame_get_it(const CallFrame* frame) {
			if (frame->args != NULL) {
				return frame->args->size ? frame->args->data[0] : NULL;
			}
			return NULL;
		}
		
		MethodCacheLine* call_frame_get_method_cache_lines(const CallFrame* frame) {
			return function_get_method_cache_lines(frame->function);
		}
		
		InstanceVariableCacheLine* call_frame_get_instance_variable_cache_lines(const CallFrame* frame) {
			return function_get_instance_variable_cache_lines(frame->function);
		}
	}
	
	class Codegen::Function : public Asm {
	public:
		typedef std::vector<Symbol> Names;
		
		Function(Codegen& codegen) :
			codegen(codegen),
			settings(codegen._settings),
			materialized_descriptor_offset(0),
			materialized_code_offset(0),
			parent(NULL),
			name(0),
			it_index(0),
			needs_environment(true),
			num_method_calls(0),
			num_instance_variable_accesses(0),
			num_temporaries(0)
		{}

		Codegen& codegen;
		const CodegenSettings& settings;
		size_t materialized_descriptor_offset;
		size_t materialized_code_offset;
		Function* parent;
		Symbol name;
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
		size_t num_method_calls;
		size_t num_instance_variable_accesses;

		int num_temporaries;
		std::vector<int> temporaries_freelist;
		int alloc_temporary();
		void free_temporary(int);
		Operand temporary(int);
		
		bool compile_function_body(const ASTNode* body_seq);
		void materialize_at(byte* destination);
	private:
		bool compile_ast_node(const ASTNode* node);
		bool compile_assignment(const ASTNode* assign);
		bool compile_call(const ASTNode* call);
		void compile_call(const Operand& functor, const Operand& self, size_t num_args, const Operand& args_ptr, size_t num_names = 0, const Operand& names_ptr = Operand());
		void compile_method_call(const Operand& self, Symbol method_name, size_t num_args, const Operand& args_ptr, size_t num_names = 0, const Operand& names_ptr = Operand());
		void compile_get_method_inline_cache(const Operand& self, Symbol name, const Register& out_type, const Register& out_method);
		void compile_get_index_of_field_inline_cache(const Operand& self, Symbol name, const Register& target, bool can_define = false);
		Operand compile_get_address_for_local(const Register& reg, Symbol name, bool can_define = false);
		Function* compile_function(const ASTNode* function);
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
	
	#define REG_CALL_FRAME   R12
	#define REG_METHOD_CACHE R13
	#define REG_IVAR_CACHE   R14
	
	bool Codegen::Function::compile_function_body(const ASTNode* body_seq) {
		pushq(RBP);
		movq(RSP, RBP);
		size_t stack_size_offset1 = subq(PLACEHOLDER_IMM32, RSP);
		
		// Back up preserved registers
		pushq(RBX);
		pushq(REG_CALL_FRAME);
		pushq(REG_METHOD_CACHE);
		pushq(REG_IVAR_CACHE);
		pushq(R15);
		
		// Set up function environment
		movq(RDI, REG_CALL_FRAME);
		CALL(ccall::call_frame_get_method_cache_lines);
		movq(RAX, REG_METHOD_CACHE);
		movq(REG_CALL_FRAME, RDI);
		CALL(ccall::call_frame_get_instance_variable_cache_lines);
		movq(RAX, REG_IVAR_CACHE);
		xorq(RAX, RAX); // always clear RAX, so empty functions return nil.
		return_label = label();
		
		if (!compile_ast_node(body_seq)) return false;
		
		// Restore preserved registers and return
		bind_label(return_label);
		popq(R15);
		popq(REG_IVAR_CACHE);
		popq(REG_METHOD_CACHE);
		popq(REG_CALL_FRAME);
		popq(RBX);
		leave();
		ret();
		
		// set stack size
		int stack_size = (num_temporaries) * sizeof(VALUE);
		size_t pad = stack_size % 16;
		if (pad != 8) stack_size += 8;
		for (size_t i = 0; i < 4; ++i) {
			code()[stack_size_offset1+i] = reinterpret_cast<byte*>(&stack_size)[i];
		}
		
		return true;
	}
	
	bool Codegen::Function::compile_ast_node(const ASTNode* node) {
		switch (node->type) {
			case ASTNodeTypeSequence: {
				for (const ASTNode* x = node->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
				}
				return true;
			}
			case ASTNodeTypeLiteral: {
				uint64_t imm = (uint64_t)node->literal.value;
				movq(imm, RAX);
				return true;
			}
			case ASTNodeTypeClosure: {
				Function* f = compile_function(node);
				if (!f) return false;
				movq(REG_CALL_FRAME, RDI);
				CALL(ccall::call_frame_environment);
				movq(RAX, RSI);
				FunctionDescriptorReference ref;
				ref.offset = movq(0, RDI);
				ref.function = f;
				function_descriptor_references.push_back(ref);
				CALL(ccall::create_function_for_descriptor);
				return true;
			}
			case ASTNodeTypeReturn: {
				if (node->return_expr.value) {
					if (!compile_ast_node(node->return_expr.value)) return false;
				}
				jmp(return_label);
				return true;
			}
			case ASTNodeTypeIdentifier: {
				Operand local_addr = compile_get_address_for_local(RDX, node->identifier.name, false);
				if (local_addr.is_valid()) {
					movq(local_addr, RAX);
				} else {
					movq(REG_CALL_FRAME, RDI);
					movq(node->identifier.name, RSI);
					CALL(ccall::local_missing);
				}
				return true;
			}
			case ASTNodeTypeSelf: {
				movq(address(REG_CALL_FRAME, UNSAFE_OFFSET_OF(CallFrame, self)), RAX);
				return true;
			}
			case ASTNodeTypeHere: {
				movq(REG_CALL_FRAME, RAX);
				return true;
			}
			case ASTNodeTypeIt: {
				movq(REG_CALL_FRAME, RDI);
				CALL(ccall::call_frame_get_it);
				return true;
			}
			case ASTNodeTypeAssign: {
				return compile_assignment(node);
			}
			case ASTNodeTypeMethod: {
				if (!compile_ast_node(node->method.object)) return false;
				Temporary self(*this);
				movq(RAX, self);
				compile_get_method_inline_cache(RAX, node->method.name, RAX, RDI);
				Label* get_method = label();
				Label* after = label();
				
				cmpb(MethodTypeFunction, RAX);
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
					CALL(ccall::create_method_proxy);
					// jmp(after);
				}
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeInstanceVariable: {
				if (!compile_ast_node(node->method.object)) return false;
				Temporary self(*this);
				movq(RAX, self);
				compile_get_index_of_field_inline_cache(RAX, node->instance_variable.name, RSI);
				movq(self, RDI);
				CALL(ccall::object_get_instance_variable_by_index);
				return true;
			}
			case ASTNodeTypeCall: {
				return compile_call(node);
			}
			case ASTNodeTypeAssociation: {
				if (!compile_ast_node(node->association.object)) return false;
				Temporary self(*this);
				movq(RAX, self);
				
				size_t num_args = node->association.args->sequence.length;
				Temporary args_ptr(*this);
				Alloca _1(*this, args_ptr, sizeof(VALUE)*num_args);
				
				size_t i = 0;
				for (ASTNode* x = node->association.args->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
					movq(args_ptr, RCX);
					movq(RAX, address(RCX, sizeof(VALUE) * i++));
				}
				compile_method_call(self, snow::sym("get"), num_args, args_ptr);
				return true;
			}
			case ASTNodeTypeAnd: {
				Label* left_true = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_and.left)) return false;
				movq(RAX, RBX);
				movq(RAX, RDI);
				CALL(snow::is_truthy);
				cmpq(0, RAX);
				j(CC_NOT_EQUAL, left_true);
				movq(RBX, RAX);
				jmp(after);
				
				bind_label(left_true);
				if (!compile_ast_node(node->logic_and.right)) return false;

				bind_label(after);
				return true;
			}
			case ASTNodeTypeOr: {
				Label* left_false = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_or.left)) return false;
				movq(RAX, RBX);
				movq(RAX, RDI);
				CALL(snow::is_truthy);
				cmpq(0, RAX);
				j(CC_EQUAL, left_false);
				movq(RBX, RAX);
				jmp(after);
				
				bind_label(left_false);
				if (!compile_ast_node(node->logic_or.right)) return false;

				bind_label(after);
				return true;
			}
			case ASTNodeTypeXor: {
				Label* equal_truth = label();
				Label* after = label();
				Label* left_is_true = label();
				
				if (!compile_ast_node(node->logic_xor.left)) return false;
				Temporary left_value(*this);
				movq(RAX, left_value); // save left value
				
				if (!compile_ast_node(node->logic_xor.right)) return false;
				movq(RAX, RBX); // save right value in caller-preserved register
				movq(RAX, RDI);
				CALL(snow::is_truthy);
				Temporary right_truth(*this);
				movq(RAX, right_truth);
				movq(left_value, RDI);
				CALL(snow::is_truthy);
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
			case ASTNodeTypeNot: {
				Label* truth = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_not.expr)) return false;
				movq(RAX, RDI);
				CALL(snow::is_truthy);
				cmpq(0, RAX);
				j(CC_NOT_EQUAL, truth);
				movq((uint64_t)SN_TRUE, RAX);
				jmp(after);
				
				bind_label(truth);
				movq((uint64_t)SN_FALSE, RAX);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeLoop: {
				Label* cond = label();
				Label* after = label();
				
				bind_label(cond);
				if (!compile_ast_node(node->loop.cond)) return false;
				movq(RAX, RDI);
				CALL(snow::is_truthy);
				cmpq(0, RAX);
				j(CC_EQUAL, after);
				
				if (!compile_ast_node(node->loop.body)) return false;
				jmp(cond);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeBreak: {
				// TODO!
				return true;
			}
			case ASTNodeTypeContinue: {
				// TODO!
				return true;
			}
			case ASTNodeTypeIfElse: {
				Label* else_body = label();
				Label* after = label();
				
				if (!compile_ast_node(node->if_else.cond)) return false;
				movq(RAX, RDI);
				CALL(snow::is_truthy);
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
	
	bool Codegen::Function::compile_assignment(const ASTNode* node) {
		ASSERT(node->assign.value->type == ASTNodeTypeSequence);
		ASSERT(node->assign.target->type == ASTNodeTypeSequence);
		
		// gather assign targets
		const size_t num_targets = node->assign.target->sequence.length;
		ASTNode* targets[num_targets];
		size_t i = 0;
		for (ASTNode* x = node->assign.target->sequence.head; x; x = x->next) {
			targets[i++] = x;
		}
		
		// allocate temporaries for this assignment
		const size_t num_values = node->assign.value->sequence.length;
		TemporaryArray values(*this, num_values);
		// compile assignment values
		i = 0;
		for (ASTNode* x = node->assign.value->sequence.head; x; x = x->next) {
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
				CALL(ccall::create_array_with_size);
				movq(RAX, RBX);
				for (size_t j = i; j < num_values; ++j) {
					movq(RBX, RDI);
					movq(values[j], RSI);
					CALL(ccall::array_push);
				}
				movq(RBX, values[i]);
			}
			
			ASTNode* target = targets[i];
			switch (target->type) {
				case ASTNodeTypeAssociation: {
					size_t num_args = target->association.args->sequence.length + 1;
					Temporary args_ptr(*this);
					Alloca _1(*this, args_ptr, sizeof(VALUE) * num_args);
					
					size_t j = 0;
					for (ASTNode* x = target->association.args->sequence.head; x; x = x->next) {
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
					compile_method_call(RAX, snow::sym("set"), num_args, args_ptr);
					break;
				}
				case ASTNodeTypeInstanceVariable: {
					if (!compile_ast_node(target->instance_variable.object)) return false;
					Temporary obj(*this);
					movq(RAX, obj);
					compile_get_index_of_field_inline_cache(RAX, target->instance_variable.name, RSI, true);
					movq(obj, RDI);
					if (i <= num_values)
						movq(values[i], RDX);
					else
						xorq(RDX, RDX);
					CALL(ccall::object_set_instance_variable_by_index);
					break;
				}
				case ASTNodeTypeIdentifier: {
					Operand local_addr = compile_get_address_for_local(RDX, target->identifier.name, true);
					ASSERT(local_addr.is_valid());
					movq(values[i], RAX);
					movq(RAX, local_addr);
					break;
				}
				case ASTNodeTypeMethod: {
					if (!compile_ast_node(target->method.object)) return false;
					movq(RAX, RDI);
					movq(target->method.name, RSI);
					if (i <= num_values)
						movq(values[i], RDX);
					else
						xorq(RDX, RDX);
					CALL(ccall::object_set_property_or_define_method);
				}
				default:
					fprintf(stderr, "CODEGEN: Invalid target for assignment! (type=%d)", target->type);
					return false;
			}
		}
		return true;
	}
	
	Codegen::Function* Codegen::Function::compile_function(const ASTNode* function) {
		ASSERT(function->type == ASTNodeTypeClosure);
		Function* f = new Function(codegen);
		f->parent = this;
		if (function->closure.parameters) {
			f->param_names.reserve(function->closure.parameters->sequence.length);
			for (const ASTNode* x = function->closure.parameters->sequence.head; x; x = x->next) {
				ASSERT(x->type == ASTNodeTypeParameter);
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
	
	Operand Codegen::Function::compile_get_address_for_local(const Register& reg, Symbol name, bool can_define) {
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
			if (level == 0) {
				// local to this scope
				movq(REG_CALL_FRAME, RDI);
				CALL(ccall::call_frame_get_locals);
				movq(RAX, reg);
				return address(reg, index * sizeof(Value));
			} else {
				// local in parent scope
				movq(REG_CALL_FRAME, RDI);
				movq(level, RSI);
				CALL(snow::get_locals_from_higher_lexical_scope);
				movq(RAX, reg);
				return address(reg, index * sizeof(Value));
			}
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
				movq(REG_CALL_FRAME, RDI);
				CALL(ccall::call_frame_get_locals);
				movq(RAX, reg);
				return address(reg, index * sizeof(Value));
			} else {
				global_idx = codegen.module_globals.size();
				codegen.module_globals.push_back(name);
				return global(global_idx);
			}
		} else {
			return Operand(); // Invalid operand.
		}
	}
	
	bool Codegen::Function::compile_call(const ASTNode* node) {
		std::vector<std::pair<const ASTNode*, int> > args;
		std::vector<Symbol> names;
		
		if (node->call.args) {
			args.reserve(node->call.args->sequence.length);

			size_t num_names = 0;
			for (const ASTNode* x = node->call.args->sequence.head; x; x = x->next) {
				if (x->type == ASTNodeTypeNamedArgument) {
					++num_names;
				}
			}
			
			size_t named_i = 0;
			size_t unnamed_i = num_names;
			names.reserve(num_names);
			for (const ASTNode* x = node->call.args->sequence.head; x; x = x->next) {
				if (x->type == ASTNodeTypeNamedArgument) {
					args.push_back(std::pair<const ASTNode*,int>(x->named_argument.expr, named_i++));
					names.push_back(x->named_argument.name);
				} else {
					args.push_back(std::pair<const ASTNode*,int>(x, unnamed_i++));
				}
			}
			ASSERT(named_i == names.size());
			ASSERT(unnamed_i == args.size());
		}
		
		Temporary args_ptr(*this);
		Temporary names_ptr(*this);
		Alloca _1(*this, args_ptr, args.size(), sizeof(VALUE));
		// TODO: Use static storage for name lists
		Alloca _2(*this, names_ptr, names.size(), sizeof(Symbol));
		
		// Compile arguments and put them in their places.
		for (size_t i = 0; i < args.size(); ++i) {
			const ASTNode* x = args[i].first;
			int idx = args[i].second;
			if (!compile_ast_node(x)) return false;
			movq(args_ptr, RDX);
			movq(RAX, address(RDX, sizeof(VALUE) * idx));
		}
		
		// Create name list.
		// TODO: Use static storage for name lists, with RIP-addressing.
		movq(names_ptr, RDX);
		for (size_t i = 0; i < names.size(); ++i) {
			movq(names[i], RAX); // cannot move a 64-bit operand directly to memory
			movq(RAX, address(RDX, sizeof(Symbol) * i));
		}
		
		if (node->call.object->type == ASTNodeTypeMethod) {
			if (!compile_ast_node(node->call.object->method.object)) return false;
			compile_method_call(RAX, node->call.object->method.name, args.size(), args_ptr, names.size(), names_ptr);
		} else {
			if (!compile_ast_node(node->call.object)) return false;
			xorq(RSI, RSI); // self = NULL
			compile_call(RAX, RSI, args.size(), args_ptr, names.size(), names_ptr);
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
			CALL(ccall::call_with_named_arguments);
		} else {
			if (num_args)
				movq(num_args, RDX);
			else
				xorq(RDX, RDX);
			movq(args_ptr, RCX);
			CALL(ccall::call);
		}
	}
	
	void Codegen::Function::compile_method_call(const Operand& in_self, Symbol method_name, size_t num_args, const Operand& args_ptr, size_t num_names, const Operand& names_ptr) {
		ASSERT(args_ptr.is_memory());
		if (num_names) ASSERT(names_ptr.is_memory());
		Temporary self(*this);
		if (in_self.is_memory()) {
			movq(in_self, RAX);
			movq(RAX, self);
		} else {
			movq(in_self, self);
		}
			
		compile_get_method_inline_cache(in_self, method_name, R8, RDI);
		movq(self, RSI);
		movq(num_args, RDX);
		movq(args_ptr, RCX);
		movq(method_name, R9);
		CALL(ccall::call_method);
	}
	
	void Codegen::Function::compile_get_method_inline_cache(const Operand& object, Symbol name, const Register& out_type, const Register& out_method_getter) {
		if (settings.use_inline_cache) {
			movq(object, RDI);
			movq(name, RSI);
			size_t cache_line = num_method_calls++;
			leaq(address(REG_METHOD_CACHE, cache_line * sizeof(MethodCacheLine)), RDX);
			Alloca _1(*this, RBX, sizeof(MethodQueryResult));
			movq(RBX, RCX);
			CALL(snow::get_method_inline_cache);
			movq(address(RBX, offsetof(MethodQueryResult, type)), out_type);
			movq(address(RBX, offsetof(MethodQueryResult, result)), out_method_getter);
		} else {
			movq(object, RDI);
			CALL(ccall::get_class);
			Alloca _1(*this, RBX, sizeof(MethodQueryResult));
			movq(RAX, RDI);
			movq(name, RSI);
			movq(RBX, RDX);
			CALL(ccall::class_lookup_method);
			movq(address(RBX, offsetof(MethodQueryResult, type)), out_type);
			movq(address(RBX, offsetof(MethodQueryResult, result)), out_method_getter);
		}
	}
	
	void Codegen::Function::compile_get_index_of_field_inline_cache(const Operand& object, Symbol name, const Register& target, bool can_define) {
		if (settings.use_inline_cache) {
			movq(object, RDI);
			movq(name, RSI);
			size_t cache_line = num_instance_variable_accesses++;
			leaq(address(REG_IVAR_CACHE, cache_line * sizeof(InstanceVariableCacheLine)), RDX);
			if (can_define)
				CALL(snow::get_or_define_instance_variable_inline_cache);
			else
				CALL(snow::get_instance_variable_inline_cache);
			movl(RAX, target);
		} else {
			movq(object, RDI);
			movq(name, RSI);
			if (can_define)
				CALL(ccall::object_get_or_create_index_of_instance_variable);
			else
				CALL(ccall::object_get_index_of_instance_variable);
			movl(RAX, target);
		}
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
	
	
	bool Codegen::compile_ast(const ASTBase* ast) {
		Function* function = new Function(*this);
		_functions.push_back(function);
		_entry = function;
		return function->compile_function_body(ast->_root);
	}
	
	size_t Codegen::compiled_size() const {
		size_t accum = module_globals.size() * sizeof(VALUE);
		for (size_t i = 0; i < _functions.size(); ++i) {
			accum += sizeof(FunctionDescriptor);
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
			
			FunctionDescriptor* descriptor = (FunctionDescriptor*)(destination + offset);
			offset += sizeof(FunctionDescriptor);
			
			descriptor->ptr = 0; // linked later
			descriptor->name = f->name;
			descriptor->return_type = AnyType;
			size_t num_params = f->param_names.size();
			descriptor->num_params = num_params;
			descriptor->param_types = (ValueType*)(destination + offset);
			offset += sizeof(ValueType) * num_params;
			descriptor->param_names = (Symbol*)(destination + offset);
			offset += sizeof(Symbol) * num_params;
			for (size_t j = 0; j < num_params; ++j) {
				descriptor->param_types[j] = AnyType; // TODO
				descriptor->param_names[j] = f->param_names[j];
			}
			
			size_t num_locals = f->local_names.size();
			descriptor->num_locals = num_locals;
			descriptor->local_names = (Symbol*)(destination + offset);
			offset += sizeof(Symbol) * num_locals;
			for (size_t j = 0; j < num_locals; ++j) {
				descriptor->local_names[j] = f->local_names[j];
			}
			
			descriptor->num_variable_references = 0; // TODO
			descriptor->variable_references = NULL;
			
			descriptor->num_method_calls = f->num_method_calls;
			descriptor->num_instance_variable_accesses = f->num_instance_variable_accesses;
		}
		
		std::vector<Function::FunctionDescriptorReference> descriptor_references;
		std::vector<Function::DirectCall> direct_calls;
		
		// materialize code
		for (size_t i = 0; i < _functions.size(); ++i) {
			Function* f = _functions[i];
			f->link_globals(offset);
			f->materialize_at(destination + offset);
			f->materialized_code_offset = offset;
			FunctionDescriptor* descriptor = (FunctionDescriptor*)(destination + f->materialized_descriptor_offset);
			descriptor->ptr = (FunctionPtr)(destination + offset);
			
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