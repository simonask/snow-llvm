#include "snow/driver/basic.hpp"
#include "snow/driver/functionpass.hpp"

using namespace llvm;

#include <stdio.h>
#include <string>

char SnowFunctionPass::ID = 0;

bool SnowFunctionPass::runOnFunction(Function& F) {
	fprintf(stderr, "SnowFunctionPass::runOnFunction\n");
	return false;
}

INITIALIZE_PASS(SnowFunctionPass, "SnowFunctionPass", "Snow Function Pass", false, false);
