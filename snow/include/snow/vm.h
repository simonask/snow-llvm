#pragma once
#ifndef VM_H_QGO3BPWD
#define VM_H_QGO3BPWD

#include "snow/basic.h"
#include "snow/symbol.h"

struct SnProcess;
struct SnAST;
struct SnFunctionDescriptor;

typedef struct SnCompilationResult {
	char* error_str; // freed by caller
	struct SnFunctionDescriptor* entry_descriptor;
} SnCompilationResult;

typedef bool(*SnCompileASTFunc)(void* vm_state, const struct SnAST* ast, SnCompilationResult* out_result);
typedef void(*SnFreeFunctionFunc)(void* vm_state, void* jit_handle);
typedef void(*SnRealizeFunctionFunc)(void* vm_state, struct SnFunctionDescriptor* descriptor);
typedef SnSymbol(*SnGetSymbolFunc)(const char* cstr);
typedef const char*(*SnGetSymbolStringFunc)(SnSymbol sym);

typedef struct SnVM {
	void* vm_state;
	SnCompileASTFunc compile_ast;
	SnFreeFunctionFunc free_function;
	SnRealizeFunctionFunc realize_function;
	
	SnGetSymbolFunc symbol;
	SnGetSymbolStringFunc symbol_to_cstr;
} SnVM;

CAPI void* snow_vm_load_precompiled_image(const char* file);
CAPI bool snow_vm_compile_ast(const struct SnAST* ast, SnCompilationResult* out_result);
CAPI void snow_vm_realize_function(struct SnFunctionDescriptor* descriptor);
CAPI void snow_vm_free_function(void* jit_handle);


#endif /* end of include guard: VM_H_QGO3BPWD */
