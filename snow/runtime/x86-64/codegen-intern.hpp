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
	
	template <typename T> struct AsmValue;
	template <typename T>
	struct AsmValue<const T*> {
		Operand op;
		AsmValue() {}
		AsmValue(const AsmValue<T*>& other) : op(other.op) {}
		AsmValue(const AsmValue<const T*>& other) : op(other.op) {}
		explicit AsmValue(const Operand& op) : op(op) {}
		operator const Operand&() const { return op; }
		operator const Register&() const { ASSERT(!op.is_memory()); return op.reg; }
		bool is_valid() const { return op.is_valid(); }
		bool is_memory() const { return op.is_memory(); }
	};
	template <typename T>
	struct AsmValue {
		Operand op;
		AsmValue() {}
		AsmValue(const AsmValue<T>& other) : op(other.op) {}
		explicit AsmValue(const Operand& op) : op(op) {}
		operator const Operand&() const { return op; }
		operator const Register&() const { ASSERT(!op.is_memory()); return op.reg; }
		bool is_valid() const { return op.is_valid(); }
		bool is_memory() const { return op.is_memory(); }
	};
	
	template <typename R, typename... Args>
	struct AsmCall {
		typedef R(*FunctionType)(Args...);
		
		Codegen::Function* caller;
		FunctionType callee;
		
		AsmCall(Codegen::Function* caller, FunctionType callee) : caller(caller), callee(callee) {}
		AsmCall(const AsmCall<R, Args...>& other) : caller(other.caller), callee(other.callee) {}
		AsmCall(AsmCall<R,Args...>&& other) : caller(other.caller), callee(other.callee) {}
		
		template <int I, typename T>
		void set_arg(T val);
		template <int I>
		void set_arg(int32_t);
		template <int I, typename T>
		void set_arg(const AsmValue<T>& val);
		template <int I, typename T>
		void set_arg(const Temporary<T>& tmp);
		template <int I>
		void clear_arg();
		
		AsmValue<R> call();
	};
	
	template <typename T>
	struct Alloca {
		Alloca(Asm& a, const AsmValue<T*>& target, size_t num_elements)
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
		operator AsmValue<T>() const;
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
		AsmValue<VALUE> temporary(int);
		
		AsmValue<VALUE> compile_function_body(const ASTNode* body_seq);
		void materialize_at(byte* destination);
		
		AsmValue<VALUE> compile_ast_node(const ASTNode* node);
		AsmValue<VALUE> compile_assignment(const ASTNode* assign);
		AsmValue<VALUE> compile_call(const ASTNode* call);
		AsmValue<VALUE> compile_call(const AsmValue<VALUE>& functor, const AsmValue<VALUE>& self, size_t num_args, const AsmValue<VALUE*>& args_ptr, size_t num_names = 0, const AsmValue<Symbol*>& names_ptr = AsmValue<Symbol*>());
		AsmValue<VALUE> compile_method_call(const AsmValue<VALUE>& self, Symbol method_name, size_t num_args, const AsmValue<VALUE*>& args_ptr, size_t num_names = 0, const AsmValue<Symbol*>& names_ptr = AsmValue<Symbol*>());
		void compile_get_method_inline_cache(const AsmValue<VALUE>& self, Symbol name, const AsmValue<MethodType>& out_type, const AsmValue<VALUE>& out_method);
		void compile_get_index_of_field_inline_cache(const AsmValue<VALUE>& self, Symbol name, const AsmValue<int32_t>& target, bool can_define = false);
		AsmValue<VALUE> compile_get_address_for_local(const Register& reg, Symbol name, bool can_define = false);
		Function* compile_function(const ASTNode* function);
		bool perform_inlining(void* callee);
		void call_direct(void* callee);
		void call_indirect(void* callee);
		
		void compile_alloca(size_t num_bytes, const Operand& out_ptr);
		
		template <typename R, typename... Args>
		AsmCall<R, Args...> call(R(*callee)(Args...)) {
			return AsmCall<R, Args...>(this, callee);
		}
		
		AsmValue<CallFrame*> get_call_frame() const {
			return AsmValue<CallFrame*>(REG_CALL_FRAME);
		}
		
		AsmValue<MethodCacheLine*> get_method_cache() const {
			return AsmValue<MethodCacheLine*>(REG_METHOD_CACHE);
		}
		
		AsmValue<InstanceVariableCacheLine*> get_ivar_cache() const {
			return AsmValue<InstanceVariableCacheLine*>(REG_IVAR_CACHE);
		}
		
		template <typename T>
		void clear(const AsmValue<T>& v) {
			ASSERT(!v.is_memory());
			xorq(v, v);
		}
	};
	
	
	template <typename R, typename... Args>
	template <int I, typename T>
	void AsmCall<R, Args...>::set_arg(T val) {
		std::tuple<Args...> args;
		std::get<I>(args) = val; // type check
		caller->movq(val, REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	template <int I>
	void AsmCall<R, Args...>::set_arg(int32_t val) {
		std::tuple<Args...> args;
		std::get<I>(args) = val; // type check
		caller->movl(val, REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	template <int I, typename T>
	void AsmCall<R, Args...>::set_arg(const AsmValue<T>& val) {
		std::tuple<AsmValue<Args>...> args;
		std::get<I>(args) = val; // type check
		caller->movq(val, REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	template <int I, typename T>
	void AsmCall<R, Args...>::set_arg(const Temporary<T>& tmp) {
		set_arg<I>(AsmValue<T>((Operand)tmp));
	}
	
	template <typename R, typename... Args>
	template <int I>
	void AsmCall<R, Args...>::clear_arg() {
		caller->xorq(REG_ARGS[I], REG_ARGS[I]);
	}
	
	template <typename R, typename... Args>
	AsmValue<R> AsmCall<R, Args...>::call() {
		caller->call_direct((void*)callee);
		return AsmValue<R>(REG_RETURN);
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
	Temporary<T>::operator AsmValue<T>() const {
		return AsmValue<T>(f.temporary(idx));
	}
}

#endif /* end of include guard: CODEGEN_INTERN_HPP_B7C5G4DG */
