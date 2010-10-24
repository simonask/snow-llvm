#include "basic.hpp"
#include "libsnow.hpp"

#include "debugger.hpp"
#include "modulepass.hpp"
#include "functionpass.hpp"

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

using namespace llvm;

static LLVMContext* context = NULL;
static ExecutionEngine* engine = NULL;
static Module* runtime = NULL;

namespace snow {
	
	void init(const char* runtime_bitcode_path) {
		ASSERT(runtime == NULL); // runtime already loaded?
		InitializeNativeTarget();
		llvm_start_multithreaded();
		context = &getGlobalContext();
		
		fprintf(stderr, "Loading runtime...\n");
		std::string error;
		MemoryBuffer* buffer = NULL;
		if ((buffer = MemoryBuffer::getFile(runtime_bitcode_path, &error))) {
			runtime = getLazyBitcodeModule(buffer, *context, &error);
			if (!runtime) {
				fprintf(stderr, "ERROR: Could not load %s.\n", runtime_bitcode_path);
				delete buffer;
				exit(1);
			}
		}
		
		engine = EngineBuilder(runtime).create();
		engine->RegisterJITEventListener(snow::debugger::jit_event_listener());
		
		//PassManager pm(m);
		//pm.add(new TargetData(*engine->getTargetData()));
		//createStandardFunctionPasses(&pm, optimization_level);
		//createStandardModulePasses(&pm, optimization_level, false, false, false, true, false, NULL);
		//createStandardLTOPasses(&pm, false, true, true);
		//pm.add(createStripSymbolsPass(true));
		//pm.add(new SnowModulePass);
		//pm.add(new SnowFunctionPass);
		//pm.run(*m);
		
		
		Function* snow_init_func = runtime->getFunction("snow_init");
		ASSERT(snow_init_func);
		engine->runStaticConstructorsDestructors(runtime, false);
		std::vector<GenericValue> args;
		args.push_back(PTOGV((void*)NULL));
		GenericValue gv_process = engine->runFunction(snow_init_func, args);
	}
	
	void finish() {
		if (engine) engine->runStaticConstructorsDestructors(runtime, true);
	}
	
	int main(int argc, const char** argv) {
		ASSERT(runtime != NULL);
		ASSERT(engine != NULL);
		GenericValue gv_ret;
		std::vector<GenericValue> args;
		GenericValue gv_argc;
		gv_argc.IntVal = APInt(32, argc);
		args.push_back(gv_argc);
		args.push_back(PTOGV((void*)argv));
		Function* snow_main_func = runtime->getFunction("snow_main");
		ASSERT(snow_main_func);
		gv_ret = engine->runFunction(snow_main_func, args);
		return gv_ret.IntVal.getSExtValue();
	}
}