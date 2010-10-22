#include "snow/driver/basic.hpp"

#include <stdio.h>

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

#include <signal.h>

#include "snow/vm-intern.hpp"
#include "snow/basic.h"

#include "snow/driver/debugger.hpp"
#include "snow/driver/modulepass.hpp"
#include "snow/driver/functionpass.hpp"

using namespace llvm;

static unsigned int optimization_level = 0;

int main (int argc, char const *argv[])
{
	JITEmitDebugInfo = true;
	
	InitializeNativeTarget();
	llvm_start_multithreaded();
	LLVMContext& context = getGlobalContext();
	
	SnVM vm;
	snow::debugger::start(&vm);

	fprintf(stderr, "Loading bitcode...\n");
	std::string error;
	Module* m = NULL;
	MemoryBuffer* buffer = NULL;
	if ((buffer = MemoryBuffer::getFile("snow.bc", &error))) {
		m = getLazyBitcodeModule(buffer, context, &error);
		if (!m) {
			fprintf(stderr, "ERROR: Could not load snow.bc.\n");
			delete buffer;
			exit(1);
		}
	}

	fprintf(stderr, "Creating EE for module %p...\n", m);
	ExecutionEngine* engine = EngineBuilder(m).create();
	engine->RegisterJITEventListener(snow::debugger::jit_event_listener());
	
	fprintf(stderr, "Running optimization passes...\n");
	//PassManager pm(m);
	//pm.add(new TargetData(*engine->getTargetData()));
	//createStandardFunctionPasses(&pm, optimization_level);
	//createStandardModulePasses(&pm, optimization_level, false, false, false, true, false, NULL);
	//createStandardLTOPasses(&pm, false, true, true);
	//pm.add(createStripSymbolsPass(true));
	//pm.add(new SnowModulePass);
	//pm.add(new SnowFunctionPass);
	//pm.run(*m);
	
	vm.context = &context;
	vm.engine = engine;
	vm.module = m;
	
	{
			fprintf(stderr, "Calling snow_init()...\n");
		Function* snow_init_func = m->getFunction("snow_init");
		ASSERT(snow_init_func);
		engine->runStaticConstructorsDestructors(m, false);
		std::vector<GenericValue> args;
		args.push_back(PTOGV((void*)&vm));
		GenericValue gv_process = engine->runFunction(snow_init_func, args);
	}

	GenericValue gv_ret;
	{
			fprintf(stderr, "Calling snow_main()...\n");
		std::vector<GenericValue> args;
		GenericValue gv_argc;
		gv_argc.IntVal = APInt(32, argc);
		args.push_back(gv_argc);
		args.push_back(PTOGV((void*)argv));
		Function* snow_main_func = m->getFunction("snow_main");
		ASSERT(snow_main_func);
		gv_ret = engine->runFunction(snow_main_func, args);
	}

	engine->runStaticConstructorsDestructors(m, true);
	return gv_ret.IntVal.getSExtValue();
}