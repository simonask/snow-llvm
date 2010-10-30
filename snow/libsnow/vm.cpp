#include "vm.hpp"
#include "codegen.hpp"

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Bitcode/ReaderWriter.h>
//#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/CodeGen/MachineCodeInfo.h>
#include <llvm/System/Process.h>
#include <llvm/PassManager.h>
#include <llvm/Support/StandardPasses.h>

#include "snow/ast.hpp"
#include "snow/function.h"

/*
	Callbacks for the runtime.
*/
static bool compile_ast(void* vm_state, const SnAST* ast, SnCompilationResult* out_result);
static void free_function(void* vm_state, void* jit_handle);
static void realize_function(void* vm_state, SnFunctionDescriptor* descriptor);

namespace snow {
	void init_vm(SnVM* vm, llvm::ExecutionEngine* ee) {
		vm->vm_state = ee;
		vm->compile_ast = compile_ast;
		vm->free_function = free_function;
		vm->realize_function = realize_function;
	}
}

bool compile_ast(void* vm_state, const SnAST* ast, SnCompilationResult* out_result) {
	llvm::ExecutionEngine* ee = (llvm::ExecutionEngine*)vm_state;
	ASSERT(ee);
	snow::Codegen codegen(llvm::getGlobalContext());
	if (codegen.compile_ast(ast)) {
		ee->addModule(codegen.get_module());
		out_result->jit_handle = codegen.get_entry_function();
		out_result->error_str = NULL;
		return true;
	} else {
		out_result->jit_handle = NULL;
		out_result->error_str = codegen.get_error_string();
		return false;
	}
}

void free_function(void* vm_state, void* jit_handle) {
	snow::JITFunction* jitf = (snow::JITFunction*)jit_handle;
	if (jitf) {
		llvm::ExecutionEngine* ee = (llvm::ExecutionEngine*)vm_state;
		ASSERT(ee);
		ee->freeMachineCodeForFunction(jitf->function);
		delete[] jitf->signature.param_types;
		delete[] jitf->signature.param_names;
		delete jitf;
	}
}

void realize_function(void* vm_state, SnFunctionDescriptor* descriptor) {
	// TODO: Run JIT on a separate thread?
	
	if (descriptor->ptr != NULL) return; // already realized
	snow::JITFunction* jitf = (snow::JITFunction*)descriptor->jit_handle;
	ASSERT(jitf);
	ASSERT(jitf->function);
	descriptor->signature = &jitf->signature;
	llvm::ExecutionEngine* ee = (llvm::ExecutionEngine*)vm_state;
	ASSERT(ee);
	llvm::MachineCodeInfo info;
	ee->runJITOnFunction(jitf->function, &info);
	descriptor->ptr = (SnFunctionPtr)info.address();
}