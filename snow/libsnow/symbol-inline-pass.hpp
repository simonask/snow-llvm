#pragma once
#ifndef SYMBOL_INLINE_PASS_HPP_2ZL629VR
#define SYMBOL_INLINE_PASS_HPP_2ZL629VR

#include <llvm/Pass.h>
#include <llvm/Function.h>

namespace snow {
	class SymbolInlinePass : public llvm::FunctionPass {
	public:
		static char ID;
		SymbolInlinePass(llvm::Function* snow_sym_function = NULL) : llvm::FunctionPass(ID), _snow_sym_function(snow_sym_function) { }
		virtual bool runOnFunction(llvm::Function& F);
	private:
		llvm::Function* _snow_sym_function;
	};
}


#endif /* end of include guard: SYMBOL_INLINE_PASS_HPP_2ZL629VR */
