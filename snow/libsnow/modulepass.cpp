#include "basic.hpp"
#include "modulepass.hpp"

using namespace llvm;

char SnowModulePass::ID = 0;

bool SnowModulePass::runOnModule(Module& M) {
	return false;
}

INITIALIZE_PASS(SnowModulePass, "SnowModulePass", "Snow Module Pass", false, false);
