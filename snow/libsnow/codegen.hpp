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

namespace snow {
	struct JITFunction {
		SnFunctionSignature signature;
		llvm::Function* function;
		SnSymbol* local_names;
		size_t num_locals;
	};
	
	struct FunctionCompilerInfo {
		FunctionCompilerInfo* parent;
		llvm::BasicBlock* function_exit;
		std::map<llvm::BasicBlock*, llvm::Value*> return_points;
		
		llvm::Value* last_value;
		llvm::Value* here;
		llvm::Value* self;
		llvm::Value* it;
		llvm::Value* locals_array;
		
		std::vector<SnSymbol> local_names;
		bool needs_context;
	};
	
	class Codegen {
	public:
		Codegen(llvm::LLVMContext& c);
		bool compile_ast(const SnAST* ast);
		
		llvm::Module* get_module() const { return _module; }
		JITFunction* get_entry_function() const { return _function; }
		char* get_error_string() const { return _error_string; }
	private:
		Codegen(llvm::LLVMContext& c, llvm::Module* module);
		
		bool compile_function_body(const SnAstNode* seq, FunctionCompilerInfo* static_parent = NULL);
		bool compile_ast_node(const SnAstNode* node, llvm::IRBuilder<>& builder, FunctionCompilerInfo& info);
		
		void gather_info_pass(const SnAstNode* node, FunctionCompilerInfo& info);
		
		/*
			find_local usage:
			
			name = name of local
			info = the compilation info for the current scope
			out_level = the number of scopes to go up to find the local
			out_adjusted_level = the number of scopes to go up, adjusted for scope elimination
			out_index = the index of the local in the designated scope
			
			returns true if a local named `name` was found, otherwise false
		*/
		bool find_local(SnSymbol name, FunctionCompilerInfo& info, int& out_level, int& out_adjusted_level, int& out_index);
		
		llvm::Value* value_constant(llvm::IRBuilder<>& builder, const VALUE constant);
		llvm::Value* symbol_constant(llvm::IRBuilder<>& builder, const SnSymbol constant);
		llvm::CallInst* method_call(llvm::IRBuilder<>& builder, FunctionCompilerInfo& info, llvm::Value* object, SnSymbol method, const std::vector<SnSymbol>& arg_names, const std::vector<llvm::Value*>& args);
		llvm::CallInst* call(llvm::IRBuilder<>& builder, FunctionCompilerInfo& info, llvm::Value* object, llvm::Value* self, const std::vector<SnSymbol>& arg_names, const std::vector<llvm::Value*>& args);
		
		llvm::CallInst* tail_call(llvm::CallInst* inst) { inst->setTailCall(true); return inst; }
		
		llvm::LLVMContext& _context;
		llvm::Module* _module;
		JITFunction* _function;
		char* _error_string;
	};
}

#endif /* end of include guard: CODEGEN_HPP_FN81609M */
