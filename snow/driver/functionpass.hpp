#pragma once
#ifndef FUNCTIONPASS_HPP_W5QDBNR0
#define FUNCTIONPASS_HPP_W5QDBNR0

#include <llvm/Pass.h>
#include <llvm/Function.h>

class SnowFunctionPass : public llvm::FunctionPass {
public:
	static char ID;
	SnowFunctionPass() : llvm::FunctionPass(ID) { }
	virtual bool runOnFunction(llvm::Function& F);
};

#endif /* end of include guard: FUNCTIONPASS_HPP_W5QDBNR0 */
