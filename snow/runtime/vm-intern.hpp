#pragma once
#ifndef VM_INTERN_H_NAVDIA6L
#define VM_INTERN_H_NAVDIA6L
/*
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>

typedef struct SnVM {
	llvm::LLVMContext* context;
	llvm::Module* module;
	llvm::ExecutionEngine* engine;
	
	llvm::Function* call;
	llvm::Function* call_with_self;
	llvm::Function* call_method;
	llvm::Function* get_member;
	llvm::Function* set_member;
	llvm::Function* get_global;
	llvm::Function* set_global;
	llvm::Function* array_get;
	llvm::Function* array_set;
	llvm::Function* arguments_init;
	llvm::Function* argument_get;
	llvm::Function* argument_push;
	llvm::Function* argument_set_named;
	llvm::Function* get_local;
	llvm::Function* set_local;
	
	const llvm::FunctionType* function_type;
	
	const llvm::Type* value_type;
	llvm::Constant* null_constant;
	llvm::Constant* nil_constant;
	llvm::Constant* true_constant;
	llvm::Constant* false_constant;
} SnVM;
*/

#endif /* end of include guard: VM_INTERN_H_NAVDIA6L */
