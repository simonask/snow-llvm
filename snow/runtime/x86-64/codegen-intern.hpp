#pragma once
#ifndef CODEGEN_INTERN_HPP_B7C5G4DG
#define CODEGEN_INTERN_HPP_B7C5G4DG

#include "codegen.hpp"
#include "cconv.hpp"
#include "asm.hpp"

#include "snow/class.hpp"
#include "snow/function.hpp"

#include <tuple>

namespace snow {
	template <typename T> struct Temporary;
	
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

#endif /* end of include guard: CODEGEN_INTERN_HPP_B7C5G4DG */
