#pragma once
#ifndef STACK_FRAME_MAP_PASS_HPP_R6T7B7YP
#define STACK_FRAME_MAP_PASS_HPP_R6T7B7YP

#include "basic.hpp"
#include <llvm/Pass.h>
#include <llvm/Function.h>

namespace snow {
	class StackFrameMapPass : public llvm::ModulePass {
	public:
		static char ID;
		StackFrameMapPass(llvm::GlobalVariable* stack_frame_chain_variable) : llvm::ModulePass(ID) {}
		virtual bool runOnModule(llvm::Module& module);
	private:
		void addStackMap(llvm::Function* f, llvm::Module* module);
	};
}

#endif /* end of include guard: STACK_FRAME_MAP_PASS_HPP_R6T7B7YP */
