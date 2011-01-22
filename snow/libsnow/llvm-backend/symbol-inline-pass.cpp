#include "basic.hpp"
#include "llvm-backend/symbol-inline-pass.hpp"
#include "symbol.hpp"

#include <llvm/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/GlobalVariable.h>
#include <llvm/Constants.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <stdio.h>
#include <string>

namespace snow {
	bool SymbolInlinePass::runOnFunction(llvm::Function& F) {
		using namespace llvm;
		bool modified = false;
		
		if (&F == _snow_sym_function) return false;
		
		for (Function::iterator bb_it = F.begin(); bb_it != F.end(); ++bb_it) {
			BasicBlock& BB = *bb_it;
			for (BasicBlock::iterator in_it = BB.begin(); in_it != BB.end(); ++in_it) {
				Instruction& instr = *in_it;
				CallInst* call = dyn_cast<CallInst>(&instr);
				if (call) {
					Function* called_function = call->getCalledFunction();
					if (called_function == _snow_sym_function && call->getNumArgOperands() == 1) {
						Constant* arg = dyn_cast<Constant>(call->getArgOperand(0));
						if (arg) {
							ConstantExpr* expr = dyn_cast<ConstantExpr>(arg);
							if (expr && expr->isGEPWithNoNotionalOverIndexing()) {
								GlobalVariable* gv = dyn_cast<GlobalVariable>(expr->getOperand(0));
								ConstantArray* str = dyn_cast<ConstantArray>(gv->getInitializer());
								std::string sym_str = str->getAsString();
								
								SnSymbol sym = snow::symbol(sym_str.c_str());
								Constant* csym = ConstantInt::get(IntegerType::get(F.getContext(), 64), sym);
								ReplaceInstWithValue(BB.getInstList(), in_it, csym);
								modified = true;
							}
						}
					}
				}
			}
		}
		
		return modified;
	}
	
	char SymbolInlinePass::ID = 0;
	using llvm::RegisterPass;
	INITIALIZE_PASS(SymbolInlinePass, "snow-symbol-inline", "Snow Symbol Inline Pass", false, false);
}
