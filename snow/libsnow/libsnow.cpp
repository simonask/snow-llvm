#include "basic.hpp"
#include "libsnow.hpp"
#include "vm.hpp"

#include "debugger.hpp"
#include "modulepass.hpp"
#include "functionpass.hpp"
#include "symbol-inline-pass.hpp"

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/System/Process.h>
#include <llvm/PassManager.h>
#include <llvm/Support/StandardPasses.h>
#include <llvm/DerivedTypes.h>

static llvm::LLVMContext* context = NULL;
static llvm::ExecutionEngine* engine = NULL;
static llvm::Module* runtime = NULL;

static SnVM vm;

namespace snow {
	void init(const char* runtime_bitcode_path) {
		ASSERT(runtime == NULL); // runtime already loaded?
		llvm::JITEmitDebugInfo = true;
		llvm::JITExceptionHandling = true;
		llvm::InitializeNativeTarget();
		llvm::llvm_start_multithreaded();
		context = &llvm::getGlobalContext();
		
		fprintf(stderr, "Loading runtime...\n");
		std::string error;
		llvm::MemoryBuffer* buffer = NULL;
		if ((buffer = llvm::MemoryBuffer::getFile(runtime_bitcode_path, &error))) {
			runtime = llvm::getLazyBitcodeModule(buffer, *context, &error);
			if (!runtime) {
				fprintf(stderr, "ERROR: Could not load %s.\n", runtime_bitcode_path);
				delete buffer;
				exit(1);
			}
		}
		
		if (runtime->MaterializeAllPermanently(&error)) {
			fprintf(stderr, "ERROR: Could not materialize runtime module! (error: %s)", error.c_str());
			exit(1);
		}
		
		engine = llvm::EngineBuilder(runtime).create();
		
		//llvm::PassManager pm;
		SymbolInlinePass sip(runtime->getFunction("snow_sym"));
		for (llvm::Module::iterator f_it = runtime->begin(); f_it != runtime->end(); ++f_it) {
			if (f_it->isDeclaration()) continue;
			sip.runOnFunction(*f_it);
		}
		//pm.add(new llvm::TargetData(*engine->getTargetData()));
		//createStandardFunctionPasses(&pm, optimization_level);
		//createStandardModulePasses(&pm, optimization_level, false, false, false, true, false, NULL);
		//createStandardLTOPasses(&pm, false, true, true);
		//pm.add(createStripSymbolsPass(true));
		//pm.add(new SnowModulePass);
		//pm.add(new SnowFunctionPass);
		//pm.run(*runtime);
		

		engine->RegisterJITEventListener(snow::debugger::jit_event_listener());
		
		snow::init_vm(&vm, engine);
		
		llvm::Function* snow_init_func = runtime->getFunction("snow_init");
		ASSERT(snow_init_func);
		engine->runStaticConstructorsDestructors(runtime, false);
		std::vector<llvm::GenericValue> args;
		args.push_back(llvm::PTOGV((void*)&vm));
		llvm::GenericValue gv_process = engine->runFunction(snow_init_func, args);
	}
	
	void finish() {
		if (engine) engine->runStaticConstructorsDestructors(runtime, true);
	}
	
	int main(int argc, const char** argv) {
		ASSERT(runtime != NULL);
		ASSERT(engine != NULL);
		llvm::GenericValue gv_ret;
		std::vector<llvm::GenericValue> args;
		llvm::GenericValue gv_argc;
		gv_argc.IntVal = llvm::APInt(32, argc);
		args.push_back(gv_argc);
		args.push_back(llvm::PTOGV((void*)argv));
		llvm::Function* snow_main_func = runtime->getFunction("snow_main");
		ASSERT(snow_main_func);
		gv_ret = engine->runFunction(snow_main_func, args);
		return gv_ret.IntVal.getSExtValue();
	}
	
	const llvm::StructType* get_runtime_struct_type(const char* name) {
		ASSERT(runtime != NULL);
		return llvm::cast<const llvm::StructType>(runtime->getTypeByName(name));
	}
	
	llvm::FunctionType* get_function_type() {
		static llvm::FunctionType* FT = NULL;
		if (!FT) {
			ASSERT(runtime != NULL); // runtime must be loaded!
			const llvm::Type* value_type = llvm::Type::getInt8PtrTy(*context);
			std::vector<const llvm::Type*> param_types(3, NULL);
			param_types[0] = llvm::PointerType::getUnqual(runtime->getTypeByName("struct.SnFunctionCallContext"));
			ASSERT(param_types[0]);
			param_types[1] = value_type;
			param_types[2] = value_type;
			FT = llvm::FunctionType::get(value_type, param_types, false);
		}
		return FT;
	}
	
	llvm::Function* get_runtime_function(const char* name) {
		ASSERT(runtime != NULL); // runtime must be loaded
		llvm::Function* F = runtime->getFunction(name);
		if (!F) {
			fprintf(stderr, "ERROR: Function '%s' not found in runtime! Codegen wants it.\n", name);
			exit(1);
		}
		return F;
	}
}