#pragma once
#ifndef CODEGEN_HPP_FN81609M
#define CODEGEN_HPP_FN81609M

#include "basic.hpp"

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include "snow/ast.hpp"
#include "snow/function.h"

#include <map>
#include <bits/stl_pair.h>
#include <stack>

struct SnString;

namespace snow {
	typedef llvm::IRBuilder<> Builder;
	
	/*
		snow::Type is used for reasoning about type inference, and only describes
		Snow types. This is opposed to llvm::Type, which describes any type.
	*/
	struct Type {
		SnType type;
		bool option;
		
		Type() : type(SnAnyType), option(false) {}
		Type(SnType type, bool option = false) : type(type), option(option) {}
		Type(const Type& other) : type(other.type), option(other.option) {}
		static Type Option(SnType type) { return Type(type, true); }
		Type& operator=(const Type& other) {
			type = other.type;
			option = other.option;
			return *this;
		}
		bool operator==(const Type& other) const {
			return type == other.type && (type == SnAnyType || type == SnNilType || type == SnFalseType || option == other.option);
		}
		bool operator==(SnType t) const { return type == t; }
		bool operator!=(SnType t) const { return type != t; }
		bool operator!=(const Type& other) const { return !(*this == other); }
		
		Type combine(const Type& other) const {
			// Get a type that is either equivalent or more general than this and other.
			if (type == other.type) return Type(type, option || other.option);
			if (type == SnNilType || type == SnFalseType) return Type(other.type, true);
			if (other.type == SnNilType || other.type == SnFalseType) return Type(type, true);
			return Type(SnAnyType);
		}
		
		bool is_numeric() const { return type == SnIntegerType || type == SnFloatType; }
		bool is_collection() const { return type == SnArrayType || type == SnMapType; }
		
		operator SnType() const { return type; }
	};
	
	/*
		snow::Value holds a Snow VALUE, where llvm::Value can hold any type of value.
		A snow::Value can always become an llvm::Value, but not all llvm::Values are
		valid snow::Values.
	*/
	struct Value {
		llvm::Value* value;
		llvm::Value* unmasked_value;
		Type type;
		
		Value() : value(NULL), unmasked_value(NULL), type(SnAnyType) {}
		Value(llvm::Value* value, Type type = SnAnyType) : value(value), unmasked_value(NULL), type(type) {
			ASSERT(!value || value->getType()->isPointerTy());
		}
		Value(llvm::Value* value, llvm::Value* unmasked_value, Type type) : value(value), unmasked_value(unmasked_value), type(type) {
			ASSERT(!value || value->getType()->isPointerTy());
		}
		Value(const Value& other) : value(other.value), unmasked_value(other.unmasked_value), type(other.type) {}
		Value& operator=(const Value& other) {
			value = other.value;
			unmasked_value = other.unmasked_value;
			type = other.type;
			return *this;
		}
		operator llvm::Value*() const { return value; }
		bool operator==(const Value& other) const { return value == other.value; }
		bool operator!=(const Value& other) const { return value != other.value; }
		bool operator<(const Value& other) const { return value < other.value; }
		llvm::Value* operator->() { return value; }
		const llvm::Value* operator->() const { return value; }
	};
	
	struct VariableReference : public SnVariableReference {
		bool operator==(const VariableReference& other) const { return level == other.level && index == other.index; }
	};
	
	struct Function {
		typedef std::map<SnSymbol, Value> LocalCache;
		typedef std::map<llvm::BasicBlock*, Value> ReturnPoints;
		typedef std::vector<SnSymbol> Names;
		typedef std::vector<Type> Types;
		
		Function* parent; // declared all up in here.
		llvm::Function* function;
		SnSymbol name;
		ReturnPoints return_points;
		llvm::BasicBlock* entry_point;
		llvm::BasicBlock* exit_point;
		llvm::BasicBlock* unwind_point;
		
		Names local_names;
		Names param_names;
		Types param_types;
		Type self_type;
		Type return_type;
		Types possible_return_types;
		std::vector<VariableReference> variable_references;
		
		Value function_value;
		Value here;
		Value self;
		Value it;
		Value module;
		llvm::Value* locals_array;
		llvm::Value* variable_references_array;
		LocalCache local_cache;
		
		int it_index;
		bool needs_environment;
		
		operator llvm::Function*() const { return function; }
		std::vector<SnType> param_types_as_simple_types() const {
			// XXX: We actually need the full type info at runtime.. Consider that.
			std::vector<SnType> r;
			r.reserve(param_types.size());
			for (size_t i = 0; i < param_types.size(); ++i) { r.push_back(param_types[i].type); }
			return r;
		}
	};
	
	class Codegen {
	public:
		Codegen(llvm::Module* runtime_module, const llvm::StringRef& module_name);
		~Codegen();
		bool compile_ast(const SnAST* ast);
		
		llvm::Module* get_module() const { return _module; }
		llvm::GlobalVariable* get_entry_descriptor() const { return _entry_descriptor; }
		char* get_error_string() const { return _error_string; }
	private:
		enum InlinableMethod {
			MethodPlus,
			MethodMinus,
			MethodMultiply,
			MethodDivide,
			MethodModulo,
			MethodComplement,
			MethodGet,
			MethodSet,
			NumInlinableMethods
		};
		struct CallArguments {
			std::vector<SnSymbol> names;
			std::vector<Value> values;
			std::vector<Value> splat_values;
			Value it;
		};
		
		Value compile_ast_node(Builder& builder, const SnAstNode* node, Function& current_function);
		Value compile_closure_body(const SnAstNode* body_sequence, Function& current_function);
		Function* compile_closure(const SnAstNode* closure, Function& declared_in);
		llvm::GlobalVariable* get_function_descriptor_for(const Function* function);
		template <typename C>
		llvm::GlobalVariable* get_constant_array(const C& container, const llvm::Twine& name);
		
		/*
			find_local usage:
			
			name = name of local
			info = the compilation info for the current scope
			out_level = the number of scopes to go up to find the local
			out_index = the index of the local in the designated scope
			
			returns true if a local named `name` was found, otherwise false
		*/
		bool find_local(SnSymbol name, Function& function, int& out_level, int& out_index);
		
		Value get_binary_numeric_operation(Builder& builder, const Value& a, const Value& b, InlinableMethod operation);
		Value get_unary_numeric_operation(Builder& builder, const Value& a, InlinableMethod operation);
		Value get_collection_operation(Builder& builder, const Value& a, const CallArguments& args, InlinableMethod operation);
		Value compile_remove_option(Builder& builder, const Value& v);
		llvm::Value* get_value_to_integer(Builder& builder, const Value& a);
		llvm::Value* get_value_to_float(Builder& builder, const Value& b);
		Value get_integer_to_value(Builder& builder, llvm::Value* n);
		Value get_float_to_value(Builder& builder, llvm::Value* f);
		llvm::Value* get_integer_to_float(Builder& builder, llvm::Value* n);
		llvm::Value* symbol(Builder& builder, SnSymbol k);
		Value constant_integer(Builder& builder, int64_t integer);
		Value constant_symbol(Builder& builder, SnSymbol symbol);
		Value constant_float(Builder& builder, float number);
		Value constant_bool(Builder& builder, bool boolean);
		Value constant_string(Builder& builder, SnString* string);
		Value nil(Builder& builder);
		Value cast_to_value(Builder& builder, const Value& value);
		
		Value compile_get_local(Builder& builder, Function& current_function, SnSymbol name);
		Value compile_association_assignment(Builder& builder, Function& current_function, const SnAstNode* association, const Value& value);
		Value compile_member_assignment(Builder& builder, Function& current_function, const SnAstNode* member, const Value& value);
		Value compile_local_assignment(Builder& builder, Function& current_function, const SnAstNode* identifier, const Value& value);
		llvm::Value* get_pointer_to_variable_reference(Builder& builder, Function& current_function, SnSymbol name);
		
		Value get_direct_method_call(Builder& builder, const Value& self, SnSymbol method, const CallArguments& args);
		Value compile_method_call(Builder& builder, Function& caller, const Value& object, SnSymbol method, const CallArguments& args);
		Value compile_call(Builder& builder, Function& caller, const Value& object, const Value& self, const CallArguments& args);
		llvm::CallInst* tail_call(llvm::CallInst* inst) const { inst->setTailCall(true); return inst; }

		llvm::InvokeInst* invoke_call(Builder& builder, Function& caller, llvm::Function* callee, llvm::Value** arg_begin, llvm::Value** arg_end, const llvm::StringRef& value_name);
		
		llvm::FunctionType* get_function_type() const;
		llvm::Function* get_runtime_function(const char* name) const;
		const llvm::StructType* get_struct_type(const char* name) const;
		const llvm::PointerType* get_pointer_to_struct_type(const char* struct_name) const;
		const llvm::PointerType* get_pointer_to_int_type(uint64_t size = 32) const;
		llvm::Value* get_null_ptr(const llvm::Type* type) const;
		const llvm::PointerType* get_value_type() const;
		
		SnSymbol current_assignment_symbol() const;
		llvm::StringRef current_assignment_name() const;
		
		llvm::Module* _runtime_module;
		llvm::Module* _module;
		llvm::StringRef _module_name;
		llvm::GlobalVariable* _entry_descriptor;
		char* _error_string;
		std::stack<SnSymbol> _assignment_name_stack;
		std::vector<SnSymbol> _module_globals;
		
		std::vector<Function*> _functions;
		std::map<const Function*, llvm::GlobalVariable*> _function_descriptors;
	};
}

#endif /* end of include guard: CODEGEN_HPP_FN81609M */
