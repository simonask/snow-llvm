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
		llvm::BasicBlock* function_exit;
		std::map<llvm::BasicBlock*, llvm::Value*> return_points;
		
		llvm::BasicBlock* current_block;
		llvm::Value* last_value;
		llvm::Value* here;
		llvm::Value* self;
		llvm::Value* it;
		
		std::vector<SnSymbol> local_names;
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
		
		bool compile_function_body(const SnAstNode* seq);
		bool compile_ast_node(const SnAstNode* node, FunctionCompilerInfo& info);
		
		llvm::Value* value_constant(llvm::IRBuilder<>& builder, const VALUE constant);
		
		llvm::LLVMContext& _context;
		llvm::Module* _module;
		JITFunction* _function;
		char* _error_string;
	};
}

#endif /* end of include guard: CODEGEN_HPP_FN81609M */
