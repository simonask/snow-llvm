#include "snow/driver/basic.hpp"
#include "snow/driver/modulepass.hpp"

using namespace llvm;

char SnowModulePass::ID = 0;

bool SnowModulePass::runOnModule(Module& M) {
	return false;
}

INITIALIZE_PASS(SnowModulePass, "SnowModulePass", "Snow Module Pass", false, false);
