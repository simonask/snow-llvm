#pragma once
#ifndef MODULEPASS_HPP_Q27417H6
#define MODULEPASS_HPP_Q27417H6

#include <llvm/Pass.h>
#include <llvm/Module.h>

class SnowModulePass : public llvm::ModulePass {
public:
	static char ID;
	SnowModulePass() : llvm::ModulePass(ID) {}
	virtual bool runOnModule(llvm::Module& M);
};

#endif /* end of include guard: MODULEPASS_HPP_Q27417H6 */
