#pragma once
#ifndef VM_H_QGO3BPWD
#define VM_H_QGO3BPWD

#include "snow/basic.h"
#include "snow/symbol.h"

struct SnProcess;
struct SnAST;
struct SnFunctionDescriptor;
struct SnString;
struct SnFunction;
struct SnFiber;

typedef struct SnCompilationResult {
	char* error_str; // freed by caller
	struct SnFunctionDescriptor* entry_descriptor;
} SnCompilationResult;

typedef bool(*SnCompileASTFunc)(void* vm_state, const char* module_name, const char* source, const struct SnAST* ast, SnCompilationResult* out_result);
typedef void(*SnFreeFunctionFunc)(void* vm_state, void* jit_handle);
typedef void(*SnRealizeFunctionFunc)(void* vm_state, struct SnFunctionDescriptor* descriptor);
typedef SnSymbol(*SnGetSymbolFunc)(const char* cstr);
typedef const char*(*SnGetSymbolStringFunc)(SnSymbol sym);
typedef int(*SnGetNameOf)(void* vm_state, void* ptr, char* buffer, int maxlen);
typedef void(*SnDisassembleFunctionFunc)(void* vm_state, const struct SnFunctionDescriptor* descriptor);
typedef void(*SnDisassembleRuntimeFunctionFunc)(void* vm_state, const char* name);

typedef VALUE(*SnModuleInitFunc)();
typedef int(*SnTestSuiteFunc)();
typedef SnModuleInitFunc(*SnLoadBitcodeModuleFunc)(void* vm_state, const char* path);

typedef VALUE(*SnFiberStartFunc)(struct SnFiber* fiber, struct SnFiber* caller, VALUE data);
typedef void(*SnFiberReturnFunc)(struct SnFiber* return_from, VALUE data);
typedef void(*SnStartFiberFunc)(struct SnFiber* fiber, struct SnFiber* caller, VALUE data, SnFiberStartFunc start_func, SnFiberReturnFunc return_callback);

typedef struct SnVM {
	void* vm_state;
	SnCompileASTFunc compile_ast;
	SnLoadBitcodeModuleFunc load_bitcode_module;
	SnDisassembleFunctionFunc disassemble_function;
	SnDisassembleRuntimeFunctionFunc disassemble_runtime_function;
	
	
	SnGetSymbolFunc symbol;
	SnGetSymbolStringFunc symbol_to_cstr;
	
	SnStartFiberFunc start_fiber;
} SnVM;

CAPI void* snow_vm_load_precompiled_image(const char* file);
CAPI bool snow_vm_compile_ast(const char* module_name, const char* soure, const struct SnAST* ast, SnCompilationResult* out_result);
CAPI struct SnObject* snow_vm_load_bitcode_module(const char* path);
CAPI struct SnString* snow_vm_get_name_of(void* ptr);
CAPI void snow_vm_disassemble_function(const struct SnFunction* function);
CAPI void snow_vm_disassemble_runtime_function(const char* name);


#endif /* end of include guard: VM_H_QGO3BPWD */
