#include "codegen.hpp"
#include "asm.hpp"
#include "snow/util.hpp"
#include "../function-internal.hpp"
#include "../inline-cache.hpp"
#include "../internal.h"

#include "snow/snow.hpp"
#include "snow/array.hpp"
#include "snow/class.hpp"

#include <map>
#include <algorithm>
#include <tuple>

#define UNSAFE_OFFSET_OF(TYPE, FIELD) (size_t)(&((const TYPE*)NULL)->FIELD)

namespace snow {
	// Define calling convention. TODO: Define for Win32 as well.
	static const Register REG_RETURN       = RAX;
	static const Register REG_ARGS[]       = { RDI, RSI, RDX, RCX, R8, R9 };
	static const Register REG_PRESERVE[]   = { RBX, R12, R13, R14, R15 };
	static const Register REG_CALL_FRAME   = R12;
	static const Register REG_METHOD_CACHE = R13;
	static const Register REG_IVAR_CACHE   = R14;
	static const Register REG_SCRATCH[]    = { R10, R11 };
	static const Register REG_PRESERVED_SCRATCH[] = { RBX, R15 };
	
	template <typename T> struct Temporary;
	template <typename T> struct TemporaryArray;
	
	template <typename T> struct ValueHolder;
	template <typename T>
	struct ValueHolder<const T*> {
		Operand op;
		ValueHolder() {}
		ValueHolder(const ValueHolder<T*>& other) : op(other.op) {}
		ValueHolder(const ValueHolder<const T*>& other) : op(other.op) {}
		explicit ValueHolder(const Operand& op) : op(op) {}
		operator const Operand&() const { return op; }
		operator const Register&() const { ASSERT(!op.is_memory()); return op.reg; }
		bool is_valid() const { return op.is_valid(); }
	};
	template <typename T>
	struct ValueHolder {
		Operand op;
		ValueHolder() {}
		ValueHolder(const ValueHolder<T>& other) : op(other.op) {}
		explicit ValueHolder(const Operand& op) : op(op) {}
		operator const Operand&() const { return op; }
		operator const Register&() const { ASSERT(!op.is_memory()); return op.reg; }
		bool is_valid() const { return op.is_valid(); }
	};
	
	template <typename R, typename... Args>
	struct Call {
		typedef R(*FunctionType)(Args...);
		
		Codegen::Function* caller;
		FunctionType callee;
		
		Call(Codegen::Function* caller, FunctionType callee) : caller(caller), callee(callee) {}
		Call(const Call<R, Args...>& other) : caller(other.caller), callee(other.callee) {}
		Call(Call<R,Args...>&& other) : caller(other.caller), callee(other.callee) {}
		
		template <int I, typename T>
		void set_arg(T val);
		template <int I>
		void set_arg(int32_t);
		template <int I, typename T>
		void set_arg(const ValueHolder<T>& val);
		template <int I, typename T>
		void set_arg(const Temporary<T>& tmp);
		template <int I>
		void clear_arg();
		
		ValueHolder<R> call();
	};
	
	template <typename T>
	struct Alloca {
		Alloca(Asm& a, const ValueHolder<T*>& target, size_t num_elements)
			: a(a), size(num_elements*sizeof(T))
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
	
	template <typename T>
	struct Temporary {
		Temporary(Codegen::Function& f);
		~Temporary();
		operator ValueHolder<T>() const;
		operator Operand() const;
		
		Codegen::Function& f;
		int idx;
	};
	
	template <typename T>
	struct TemporaryArray {
		TemporaryArray(Codegen::Function& func, size_t size) : size(size) {
			elements = new Element[size];
			construct_range(reinterpret_cast<Temporary<T>*>(elements), size, func);
		}
		~TemporaryArray() {
			destruct_range(reinterpret_cast<Temporary<T>*>(elements), size);
			delete[] elements;
		}
		Temporary<T>& operator[](size_t idx) { return *(reinterpret_cast<Temporary<T>*>(elements + idx)); }
		
		size_t size;
	private:
		typedef Placeholder<Temporary<T> > Element;
		Element* elements;
	};
}

namespace snow {
	namespace ccall {
		void class_lookup_method(Object* cls, Symbol name, MethodQueryResult* out_method) {
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
		
		void array_push(Object* array, VALUE value) {
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
		ValueHolder<VALUE> temporary(int);
		
		bool compile_function_body(const ASTNode* body_seq);
		void materialize_at(byte* destination);
		
		bool compile_ast_node(const ASTNode* node);
		bool compile_assignment(const ASTNode* assign);
		bool compile_call(const ASTNode* call);
		void compile_call(const ValueHolder<VALUE>& functor, const ValueHolder<VALUE>& self, size_t num_args, const ValueHolder<VALUE*>& args_ptr, size_t num_names = 0, const ValueHolder<Symbol*>& names_ptr = ValueHolder<Symbol*>());
		void compile_method_call(const ValueHolder<VALUE>& self, Symbol method_name, size_t num_args, const ValueHolder<VALUE*>& args_ptr, size_t num_names = 0, const ValueHolder<Symbol*>& names_ptr = ValueHolder<Symbol*>());
		void compile_get_method_inline_cache(const ValueHolder<VALUE>& self, Symbol name, const ValueHolder<MethodType>& out_type, const ValueHolder<VALUE>& out_method);
		void compile_get_index_of_field_inline_cache(const ValueHolder<VALUE>& self, Symbol name, const ValueHolder<int32_t>& target, bool can_define = false);
		ValueHolder<VALUE*> compile_get_address_for_local(const Register& reg, Symbol name, bool can_define = false);
		Function* compile_function(const ASTNode* function);
		bool perform_inlining(void* callee);
		void call_direct(void* callee);
		void call_indirect(void* callee);
		
		void compile_alloca(size_t num_bytes, const Operand& out_ptr);
		
		template <typename R, typename... Args>
		Call<R, Args...> call(R(*callee)(Args...)) {
			return Call<R, Args...>(this, callee);
		}
		
		ValueHolder<CallFrame*> get_call_frame() const {
			return ValueHolder<CallFrame*>(REG_CALL_FRAME);
		}
		
		ValueHolder<MethodCacheLine*> get_method_cache() const {
			return ValueHolder<MethodCacheLine*>(REG_METHOD_CACHE);
		}
		
		ValueHolder<InstanceVariableCacheLine*> get_ivar_cache() const {
			return ValueHolder<InstanceVariableCacheLine*>(REG_IVAR_CACHE);
		}
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
	
	inline ValueHolder<VALUE> Codegen::Function::temporary(int idx) {
		return ValueHolder<VALUE>(address(RBP, -(idx+1)*sizeof(VALUE)));
	}
	
	bool Codegen::Function::compile_function_body(const ASTNode* body_seq) {
		pushq(RBP);
		movq(RSP, RBP);
		size_t stack_size_offset1 = subq(PLACEHOLDER_IMM32, RSP);
		
		// Back up preserved registers
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			pushq(REG_PRESERVE[i]);
		}
		
		// Set up function environment
		movq(REG_ARGS[0], REG_CALL_FRAME);
		
		auto c1 = call(ccall::call_frame_get_method_cache_lines);
		c1.set_arg<0>(get_call_frame());
		auto method_cache = c1.call();
		movq(method_cache, REG_METHOD_CACHE);
		
		auto c2 = call(ccall::call_frame_get_instance_variable_cache_lines);
		c2.set_arg<0>(get_call_frame());
		auto ivar_cache = c2.call();
		movq(ivar_cache, REG_IVAR_CACHE);
		
		xorq(REG_RETURN, REG_RETURN); // always clear return register, so empty functions return nil.
		return_label = label();
		
		if (!compile_ast_node(body_seq)) return false;
		
		// Restore preserved registers and return
		bind_label(return_label);
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			popq(REG_PRESERVE[countof(REG_PRESERVE)-i-1]);
		}
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
				movq(imm, REG_RETURN);
				return true;
			}
			case ASTNodeTypeClosure: {
				Function* f = compile_function(node);
				if (!f) return false;
				
				auto c_env = call(ccall::call_frame_environment);
				c_env.set_arg<0>(get_call_frame());
				auto env = c_env.call();
				FunctionDescriptorReference ref;
				ValueHolder<const FunctionDescriptor*> desc(REG_ARGS[0]);
				ref.offset = movq(PLACEHOLDER_IMM64, REG_ARGS[0]);
				ref.function = f;
				function_descriptor_references.push_back(ref);
				auto c_func = call(ccall::create_function_for_descriptor);
				c_func.set_arg<0>(desc);
				c_func.set_arg<1>(env);
				c_func.call();
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
				Operand local_addr = compile_get_address_for_local(REG_ARGS[2], node->identifier.name, false);
				if (local_addr.is_valid()) {
					movq(local_addr, REG_RETURN);
				} else {
					auto c_local_missing = call(ccall::local_missing);
					c_local_missing.set_arg<0>(get_call_frame());
					c_local_missing.set_arg<1>(node->identifier.name);
					c_local_missing.call();
				}
				return true;
			}
			case ASTNodeTypeSelf: {
				movq(address(REG_CALL_FRAME, UNSAFE_OFFSET_OF(CallFrame, self)), REG_RETURN);
				return true;
			}
			case ASTNodeTypeHere: {
				movq(REG_CALL_FRAME, REG_RETURN);
				return true;
			}
			case ASTNodeTypeIt: {
				auto c_it = call(ccall::call_frame_get_it);
				c_it.set_arg<0>(get_call_frame());
				c_it.call();
				return true;
			}
			case ASTNodeTypeAssign: {
				return compile_assignment(node);
			}
			case ASTNodeTypeMethod: {
				if (!compile_ast_node(node->method.object)) return false;
				ValueHolder<VALUE> result(REG_RETURN);
				Temporary<VALUE> self(*this);
				movq(result, self);
				
				ValueHolder<MethodType> method_type(RAX);
				ValueHolder<VALUE> method(REG_ARGS[0]);
				compile_get_method_inline_cache(result, node->method.name, method_type, method);
				Label* get_method = label();
				Label* after = label();
				
				cmpb(MethodTypeFunction, method_type);
				j(CC_EQUAL, get_method);
				
				{
					// get property
					ValueHolder<VALUE*> args(REG_ARGS[3]);
					xorq(args, args); // no args!
					compile_call(method, self, 0, args);
					jmp(after);
				}
				
				{
					// get method proxy wrapper
					bind_label(get_method);
					auto c_create_method_proxy = call(ccall::create_method_proxy);
					c_create_method_proxy.set_arg<0>(self);
					c_create_method_proxy.set_arg<1>(method);
					c_create_method_proxy.call();
					// jmp(after);
				}
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeInstanceVariable: {
				if (!compile_ast_node(node->method.object)) return false;
				ValueHolder<VALUE> result(REG_RETURN);
				Temporary<VALUE> self(*this);
				movq(result, self);
				ValueHolder<int32_t> idx(REG_ARGS[1]);
				compile_get_index_of_field_inline_cache(result, node->instance_variable.name, idx);
				auto c_object_get_ivar_by_index = call(ccall::object_get_instance_variable_by_index);
				c_object_get_ivar_by_index.set_arg<0>(self);
				c_object_get_ivar_by_index.set_arg<1>(idx);
				c_object_get_ivar_by_index.call();
				return true;
			}
			case ASTNodeTypeCall: {
				return compile_call(node);
			}
			case ASTNodeTypeAssociation: {
				if (!compile_ast_node(node->association.object)) return false;
				ValueHolder<VALUE> result(REG_RETURN);
				Temporary<VALUE> self(*this);
				movq(result, self);
				
				size_t num_args = node->association.args->sequence.length;
				Temporary<VALUE*> args_ptr(*this);
				Alloca<VALUE> _1(*this, args_ptr, num_args);
				
				size_t i = 0;
				for (ASTNode* x = node->association.args->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
					movq(args_ptr, REG_SCRATCH[0]);
					movq(result, address(REG_SCRATCH[0], sizeof(VALUE) * i++));
				}
				compile_method_call(self, snow::sym("get"), num_args, args_ptr);
				return true;
			}
			case ASTNodeTypeAnd: {
				Label* left_true = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_and.left)) return false;
				ValueHolder<VALUE> result(REG_RETURN);
				movq(result, REG_PRESERVED_SCRATCH[0]);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_NOT_EQUAL, left_true);
				movq(REG_PRESERVED_SCRATCH[0], REG_RETURN);
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
				ValueHolder<VALUE> result(REG_RETURN);
				movq(result, REG_PRESERVED_SCRATCH[0]);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, left_false);
				movq(REG_PRESERVED_SCRATCH[0], result);
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
				ValueHolder<VALUE> result(REG_RETURN);
				Temporary<VALUE> left_value(*this);
				movq(result, left_value); // save left value
				
				if (!compile_ast_node(node->logic_xor.right)) return false;
				movq(result, REG_PRESERVED_SCRATCH[0]); // save right value in caller-preserved register
				auto c_is_truthy1 = call(snow::is_truthy);
				c_is_truthy1.set_arg<0>(result);
				auto truthy1 = c_is_truthy1.call();
				Temporary<bool> right_truth(*this);
				movq(truthy1, right_truth);
				auto c_is_truthy2 = call(snow::is_truthy);
				c_is_truthy2.set_arg<0>(left_value);
				auto truthy2 = c_is_truthy2.call();
				cmpb(truthy2, right_truth); // compare left truth to right truth
				j(CC_EQUAL, equal_truth);
				cmpq(0, truthy2); // compare left truth to zero
				j(CC_NOT_EQUAL, left_is_true);
				movq(REG_PRESERVED_SCRATCH[0], result); // previously saved right value
				jmp(after);
				
				bind_label(left_is_true);
				movq(left_value, result);
				jmp(after);
				
				bind_label(equal_truth);
				movq((uintptr_t)SN_FALSE, result);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeNot: {
				Label* truth = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_not.expr)) return false;
				ValueHolder<VALUE> result(REG_RETURN);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_NOT_EQUAL, truth);
				movq((uintptr_t)SN_TRUE, result);
				jmp(after);
				
				bind_label(truth);
				movq((uintptr_t)SN_FALSE, result);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeLoop: {
				Label* cond = label();
				Label* after = label();
				
				bind_label(cond);
				if (!compile_ast_node(node->loop.cond)) return false;
				ValueHolder<VALUE> result(REG_RETURN);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
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
				ValueHolder<VALUE> result(REG_RETURN);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
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
		TemporaryArray<VALUE> values(*this, num_values);
		// compile assignment values
		i = 0;
		for (ASTNode* x = node->assign.value->sequence.head; x; x = x->next) {
			// TODO: Assignment name
			if (!compile_ast_node(x)) return false;
			movq(REG_RETURN, values[i++]);
		}
		
		// assign values to targets
		for (size_t i = 0; i < num_targets; ++i) {
			// If this is the last target, and there are several remaining
			// values, put the rest of them in an array, and assign that.
			if (i == num_targets-1 && num_values > num_targets) {
				size_t num_remaining = num_values - num_targets + 1;
				auto c_create_array_with_size = call(ccall::create_array_with_size);
				c_create_array_with_size.set_arg<0>(num_remaining);
				auto array_r = c_create_array_with_size.call();
				ValueHolder<Object*> array(REG_PRESERVED_SCRATCH[0]);
				movq(array_r, REG_PRESERVED_SCRATCH[0]);
				for (size_t j = i; j < num_values; ++j) {
					auto c_array_push = call(ccall::array_push);
					c_array_push.set_arg<0>(array);
					c_array_push.set_arg<1>(values[j]);
					c_array_push.call();
				}
				movq(array, values[i]);
			}
			
			ASTNode* target = targets[i];
			switch (target->type) {
				case ASTNodeTypeAssociation: {
					size_t num_args = target->association.args->sequence.length + 1;
					Temporary<VALUE*> args_ptr(*this);
					Alloca<VALUE> _1(*this, args_ptr, num_args);
					
					size_t j = 0;
					for (ASTNode* x = target->association.args->sequence.head; x; x = x->next) {
						if (!compile_ast_node(x)) return false;
						movq(args_ptr, REG_ARGS[3]);
						movq(REG_RETURN, address(REG_ARGS[3], sizeof(VALUE) * j++));
					}
					if (!j) movq(args_ptr, REG_ARGS[3]); // if j != 0, args_ptr is already in RCX from above
					if (i < num_values)
						movq(values[i], REG_ARGS[2]);
					else
						movq((uint64_t)SN_NIL, REG_ARGS[2]);
					movq(REG_ARGS[2], address(REG_ARGS[3], sizeof(VALUE) * j));
					
					if (!compile_ast_node(target->association.object)) return false;
					compile_method_call(ValueHolder<VALUE>(REG_RETURN), snow::sym("set"), num_args, args_ptr);
					break;
				}
				case ASTNodeTypeInstanceVariable: {
					if (!compile_ast_node(target->instance_variable.object)) return false;
					ValueHolder<VALUE> result(REG_RETURN);
					Temporary<VALUE> obj(*this);
					movq(result, obj);
					ValueHolder<int32_t> idx(REG_ARGS[1]);
					compile_get_index_of_field_inline_cache(result, target->instance_variable.name, idx, true);
					
					auto c_object_set_ivar_by_index = call(ccall::object_set_instance_variable_by_index);
					c_object_set_ivar_by_index.set_arg<0>(obj);
					c_object_set_ivar_by_index.set_arg<1>(idx);
					if (i <= num_values)
						c_object_set_ivar_by_index.set_arg<2>(values[i]);
					else
						c_object_set_ivar_by_index.clear_arg<2>();
					c_object_set_ivar_by_index.call();
					break;
				}
				case ASTNodeTypeIdentifier: {
					ValueHolder<VALUE*> local_addr = compile_get_address_for_local(REG_SCRATCH[0], target->identifier.name, true);
					ASSERT(local_addr.is_valid());
					movq(values[i], REG_RETURN);
					movq(REG_RETURN, local_addr);
					break;
				}
				case ASTNodeTypeMethod: {
					if (!compile_ast_node(target->method.object)) return false;
					ValueHolder<VALUE> result(REG_RETURN);
					auto c_object_set = call(ccall::object_set_property_or_define_method);
					c_object_set.set_arg<0>(result);
					c_object_set.set_arg<1>(target->method.name);
					if (i <= num_values)
						c_object_set.set_arg<2>(values[i]);
					else
						c_object_set.clear_arg<2>();
					c_object_set.call();
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
	
	ValueHolder<VALUE*> Codegen::Function::compile_get_address_for_local(const Register& reg, Symbol name, bool can_define) {
		// Look for locals
		Function* f = this;
		int32_t level = 0;
		int32_t index = -1;
		while (f) {
			index = index_of(f->local_names, name);
			if (index >= 0) break;
			f = f->parent;
			++level;
		}
		if (index >= 0) {
			if (level == 0) {
				// local to this scope
				auto c_get_locals = call(ccall::call_frame_get_locals);
				c_get_locals.set_arg<0>(get_call_frame());
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return ValueHolder<VALUE*>(address(reg, index * sizeof(Value)));
			} else {
				// local in parent scope
				auto c_get_locals = call(snow::get_locals_from_higher_lexical_scope);
				c_get_locals.set_arg<0>(get_call_frame());
				c_get_locals.set_arg<1>(level);
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return ValueHolder<VALUE*>(address(reg, index * sizeof(Value)));
			}
		}
		
		// Look for module globals.
		ssize_t global_idx = index_of(codegen.module_globals, name);
		if (global_idx >= 0) {
			return ValueHolder<VALUE*>(global(global_idx));
		}
		
		if (can_define) {
			if (parent) {
				// if we're not in module global scope, define a regular local
				index = local_names.size();
				local_names.push_back(name);
				auto c_get_locals = call(ccall::call_frame_get_locals);
				c_get_locals.set_arg<0>(get_call_frame());
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return ValueHolder<VALUE*>(address(reg, index * sizeof(Value)));
			} else {
				global_idx = codegen.module_globals.size();
				codegen.module_globals.push_back(name);
				return ValueHolder<VALUE*>(global(global_idx));
			}
		} else {
			return ValueHolder<VALUE*>(); // Invalid operand.
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
		
		Temporary<VALUE*> args_ptr(*this);
		Temporary<Symbol*> names_ptr(*this);
		Alloca<VALUE> _1(*this, args_ptr, args.size());
		// TODO: Use static storage for name lists
		Alloca<Symbol> _2(*this, names_ptr, names.size());
		
		// Compile arguments and put them in their places.
		for (size_t i = 0; i < args.size(); ++i) {
			const ASTNode* x = args[i].first;
			int idx = args[i].second;
			if (!compile_ast_node(x)) return false;
			movq(args_ptr, REG_SCRATCH[0]);
			movq(REG_RETURN, address(REG_SCRATCH[0], sizeof(VALUE) * idx));
		}
		
		// Create name list.
		// TODO: Use static storage for name lists, with RIP-addressing.
		movq(names_ptr, REG_ARGS[2]);
		for (size_t i = 0; i < names.size(); ++i) {
			movq(names[i], REG_SCRATCH[0]); // cannot move a 64-bit operand directly to memory
			movq(REG_SCRATCH[0], address(REG_ARGS[2], sizeof(Symbol) * i));
		}
		
		if (node->call.object->type == ASTNodeTypeMethod) {
			if (!compile_ast_node(node->call.object->method.object)) return false;
			ValueHolder<VALUE> result(REG_RETURN);
			compile_method_call(result, node->call.object->method.name, args.size(), args_ptr, names.size(), names_ptr);
		} else {
			if (!compile_ast_node(node->call.object)) return false;
			ValueHolder<VALUE> result(REG_RETURN);
			ValueHolder<VALUE> self(REG_ARGS[1]);
			xorq(self, self); // self = NULL
			compile_call(result, self, args.size(), args_ptr, names.size(), names_ptr);
		}
		return true;
	}
	
	void Codegen::Function::compile_call(const ValueHolder<VALUE>& functor, const ValueHolder<VALUE>& self, size_t num_args, const ValueHolder<VALUE*>& args_ptr, size_t num_names, const ValueHolder<Symbol*>& names_ptr) {
		if (num_names) {
			ASSERT(num_args >= num_names);
			auto c_call = call(ccall::call_with_named_arguments);
			c_call.set_arg<0>(functor);
			c_call.set_arg<1>(self);
			c_call.set_arg<2>(num_names);
			c_call.set_arg<3>(names_ptr);
			c_call.set_arg<4>(num_args);
			c_call.set_arg<5>(args_ptr);
			c_call.call();
		} else {
			auto c_call = call(ccall::call);
			c_call.set_arg<0>(functor);
			c_call.set_arg<1>(self);
			if (num_args)
				c_call.set_arg<2>(num_args);
			else
				c_call.clear_arg<2>();
			c_call.set_arg<3>(args_ptr);
			c_call.call();
		}
	}
	
	void Codegen::Function::compile_method_call(const ValueHolder<VALUE>& in_self, Symbol method_name, size_t num_args, const ValueHolder<VALUE*>& args_ptr, size_t num_names, const ValueHolder<Symbol*>& names_ptr) {
		ASSERT(args_ptr.op.is_memory());
		if (num_names) ASSERT(names_ptr.op.is_memory());
		Temporary<VALUE> self(*this);
		if (in_self.op.is_memory()) {
			movq(in_self, REG_SCRATCH[0]);
			movq(REG_SCRATCH[0], self);
		} else {
			movq(in_self, self);
		}
		
		ValueHolder<VALUE> method(REG_ARGS[0]);
		ValueHolder<MethodType> type(REG_ARGS[4]);
		compile_get_method_inline_cache(in_self, method_name, type, method);
		auto c_call = call(ccall::call_method);
		c_call.set_arg<0>(method);
		c_call.set_arg<1>(self);
		c_call.set_arg<2>(num_args);
		c_call.set_arg<3>(args_ptr);
		c_call.set_arg<4>(type);
		c_call.set_arg<5>(method_name);
		c_call.call();
	}
	
	void Codegen::Function::compile_get_method_inline_cache(const ValueHolder<VALUE>& object, Symbol name, const ValueHolder<MethodType>& out_type, const ValueHolder<VALUE>& out_method_getter) {
		if (settings.use_inline_cache) {
			auto c_get_method = call(snow::get_method_inline_cache);
			c_get_method.set_arg<0>(object);
			c_get_method.set_arg<1>(name);
			
			size_t cache_line = num_method_calls++;
			leaq(address(REG_METHOD_CACHE, cache_line * sizeof(MethodCacheLine)), REG_ARGS[2]);
			c_get_method.set_arg<2>(ValueHolder<MethodCacheLine*>(REG_ARGS[2]));
			
			ValueHolder<MethodQueryResult*> result_ptr(REG_PRESERVED_SCRATCH[0]);
			Alloca<MethodQueryResult> _1(*this, result_ptr, 1);
			c_get_method.set_arg<3>(result_ptr);
			
			c_get_method.call();
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, type)), out_type);
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, result)), out_method_getter);
		} else {
			auto c_get_class = call(ccall::get_class);
			c_get_class.set_arg<0>(object);
			auto cls = c_get_class.call();
			
			auto c_lookup_method = call(ccall::class_lookup_method);
			c_lookup_method.set_arg<0>(cls);
			c_lookup_method.set_arg<1>(name);
			ValueHolder<MethodQueryResult*> result_ptr(REG_PRESERVED_SCRATCH[0]);
			Alloca<MethodQueryResult> _1(*this, result_ptr, 1);
			c_lookup_method.set_arg<2>(result_ptr);
			
			c_lookup_method.call();
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, type)), out_type);
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, result)), out_method_getter);
		}
	}
	
	void Codegen::Function::compile_get_index_of_field_inline_cache(const ValueHolder<VALUE>& object, Symbol name, const ValueHolder<int32_t>& target, bool can_define) {
		if (settings.use_inline_cache) {
			auto c_get_ivar_index = call(snow::get_instance_variable_inline_cache);
			c_get_ivar_index.set_arg<0>(object);
			c_get_ivar_index.set_arg<1>(name);
			size_t cache_line = num_instance_variable_accesses++;
			leaq(address(REG_IVAR_CACHE, cache_line * sizeof(InstanceVariableCacheLine)), REG_ARGS[2]);
			c_get_ivar_index.set_arg<2>(ValueHolder<InstanceVariableCacheLine*>(REG_ARGS[2]));
			if (can_define)
				c_get_ivar_index.callee = snow::get_or_define_instance_variable_inline_cache;
			auto idx = c_get_ivar_index.call();
			movl(idx, target);
		} else {
			auto c_get_ivar_index = call(ccall::object_get_index_of_instance_variable);
			c_get_ivar_index.set_arg<0>(object);
			c_get_ivar_index.set_arg<1>(name);
			if (can_define)
				c_get_ivar_index.callee = ccall::object_get_or_create_index_of_instance_variable;
			auto idx = c_get_ivar_index.call();
			movl(idx, target);
		}
	}
	
	bool Codegen::Function::perform_inlining(void* callee) {
		if (settings.perform_inlining) {
			if (callee == snow::is_truthy) {
				// val != NULL && val != SN_NIL && val != SN_FALSE;
				auto input = REG_ARGS[0];
				auto scratch = REG_SCRATCH[0];
				Label* done = label();
				
				movq(input, scratch);
				orq((uintptr_t)SN_NIL, scratch);
				xorb(REG_RETURN, REG_RETURN);
				cmpq((uintptr_t)SN_NIL, scratch);
				j(CC_EQUAL, done);
				cmpq((uintptr_t)SN_FALSE, input);
				setb(CC_NOT_EQUAL, REG_RETURN);
				bind_label(done);
				
				return true;
			}
		}
		return false;
	}
	
	void Codegen::Function::call_direct(void* callee) {
		if (!perform_inlining(callee)) {
			DirectCall dc;
			dc.offset = Asm::call(0);
			dc.callee = callee;
			direct_calls.push_back(dc);
		}
	}
	
	void Codegen::Function::call_indirect(void* callee) {
		movq((uintptr_t)callee, REG_SCRATCH[0]);
		Asm::call(REG_SCRATCH[0]);
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
	
	
	template <typename R, typename... Args>
	template <int I, typename T>
	void Call<R, Args...>::set_arg(T val) {
		std::tuple<Args...> args;
		std::get<I>(args) = val; // type check
		caller->movq(val, REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	template <int I>
	void Call<R, Args...>::set_arg(int32_t val) {
		std::tuple<Args...> args;
		std::get<I>(args) = val; // type check
		caller->movl(val, REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	template <int I, typename T>
	void Call<R, Args...>::set_arg(const ValueHolder<T>& val) {
		std::tuple<ValueHolder<Args>...> args;
		std::get<I>(args) = val; // type check
		caller->movq(val, REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	template <int I, typename T>
	void Call<R, Args...>::set_arg(const Temporary<T>& tmp) {
		set_arg<I>(ValueHolder<T>((Operand)tmp));
	}
	
	template <typename R, typename... Args>
	template <int I>
	void Call<R, Args...>::clear_arg() {
		caller->xorq(REG_ARGS[I], REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	ValueHolder<R> Call<R, Args...>::call() {
		caller->call_direct((void*)callee);
		return ValueHolder<R>(REG_RETURN);
	}

	template <typename T>
	Temporary<T>::Temporary(Codegen::Function& f) : f(f) {
		idx = f.alloc_temporary();
	}
	
	template <typename T>
	Temporary<T>::~Temporary() {
		f.free_temporary(idx);
	}
	
	template <typename T>
	Temporary<T>::operator Operand() const {
		return f.temporary(idx);
	}
	
	template <typename T>
	Temporary<T>::operator ValueHolder<T>() const {
		return ValueHolder<T>(f.temporary(idx));
	}
}