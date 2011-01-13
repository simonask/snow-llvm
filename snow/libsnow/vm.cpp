#include "vm.hpp"
#include "codegen.hpp"
#include "symbol.hpp"

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
static bool compile_ast(void* vm_state, const char* module_name, const char* source, const SnAST* ast, SnCompilationResult* out_result);
static void free_function(void* vm_state, void* jit_handle);
static void realize_function(void* vm_state, SnFunctionDescriptor* descriptor);
static int get_name_of(void* vm_state, void* ptr, char* buffer, int maxlength);
static SnModuleInitFunc load_bitcode_module(void* vm_state, const char* path);

namespace snow {
	void init_vm(SnVM* vm, llvm::ExecutionEngine* ee) {
		vm->vm_state = ee;
		vm->compile_ast = compile_ast;
		vm->free_function = free_function;
		vm->realize_function = realize_function;
		vm->get_name_of = get_name_of;
		vm->load_bitcode_module = load_bitcode_module;
		
		vm->symbol = snow::symbol;
		vm->symbol_to_cstr = snow::symbol_to_cstr;
	}
}

bool compile_ast(void* vm_state, const char* module_name, const char* source, const SnAST* ast, SnCompilationResult* out_result) {
	llvm::ExecutionEngine* ee = (llvm::ExecutionEngine*)vm_state;
	ASSERT(ee);
	snow::Codegen codegen(llvm::getGlobalContext(), module_name);
	if (codegen.compile_ast(ast)) {
		llvm::Module* m = codegen.get_module();
		ee->addModule(m);
		//llvm::outs() << *m << '\n';
		ee->runStaticConstructorsDestructors(m, false);
		ee->runJITOnFunction(m->getFunction("snow_module_entry"));
		llvm::GlobalVariable* entry = codegen.get_entry_descriptor();
		SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)ee->getOrEmitGlobalVariable(entry);
		out_result->entry_descriptor = descriptor;
		out_result->error_str = NULL;
		return true;
	} else {
		out_result->entry_descriptor = NULL;
		out_result->error_str = codegen.get_error_string();
		return false;
	}
}

void free_function(void* vm_state, void* jit_handle) {

}

void realize_function(void* vm_state, SnFunctionDescriptor* descriptor) {
	// TODO: Run JIT on a separate thread?
}

int get_name_of(void* vm_state, void* ptr, char* buffer, int maxlen) {
	llvm::ExecutionEngine* ee = (llvm::ExecutionEngine*)vm_state;
	if (ee->isSymbolSearchingDisabled()) return 0;
	const llvm::GlobalValue* gv = ee->getGlobalValueAtAddress(ptr);
	if (gv) {
		llvm::StringRef name = gv->getName();
		int copy = name.size();
		copy = copy > maxlen ? maxlen : copy;
		memcpy(buffer, gv->getName().data(), copy);
		return copy;
	}
	return 0;
}

SnModuleInitFunc load_bitcode_module(void* vm_state, const char* path) {
	llvm::ExecutionEngine* ee = (llvm::ExecutionEngine*)vm_state;
	ASSERT(ee);
	
	std::string error;
	llvm::MemoryBuffer* buffer = NULL;
	llvm::Module* module = NULL;
	if ((buffer = llvm::MemoryBuffer::getFile(path, &error))) {
		module = llvm::getLazyBitcodeModule(buffer, llvm::getGlobalContext(), &error);
		if (!module) {
			fprintf(stderr, "ERROR: Could not load %s.\n", path);
			delete buffer;
			return NULL;
		}
	}
	
	llvm::Function* init_func = module->getFunction("snow_module_init");
	if (!init_func) {
		fprintf(stderr, "ERROR: Module does not contain a function named 'snow_module_init'.\n");
		return NULL;
	}
	
	ee->addModule(module);
	ee->runStaticConstructorsDestructors(module, false);
	return (SnModuleInitFunc)ee->getPointerToFunction(init_func);
}