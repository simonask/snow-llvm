#include "llvm-backend/stack-frame-map-pass.hpp"

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/Type.h>

namespace snow {
	bool StackFrameMapPass::runOnModule(llvm::Module& module) {
		using namespace llvm;
		
		for (Module::iterator it = module.begin(); it != module.end(); ++it) {
			Function* f = &*it;
			if (!f->isDeclaration() && !f->isIntrinsic()) {
				addStackMap(f, &module);
			}
		}
		return false;
	}
	
	void StackFrameMapPass::addStackMap(llvm::Function* f, llvm::Module* module) {
		using namespace llvm;
		
		for (Function::iterator it = f->begin(); it != f->end(); ++it) {
			BasicBlock* bb = &*it;
			
			for (BasicBlock::iterator it2 = bb->begin(); it2 != bb->end(); ++it2) {
				Instruction* inst = &*it2;
				const Type* type = inst->getType();
				if (type->getNumContainedTypes() > 0 && type->isPointerTy()) {
					const Type* contained_type = type->getContainedType(0);
					if (contained_type->isVoidTy()
					 || contained_type->isStructTy()
					 || contained_type->isOpaqueTy()) {
						// might be a GC root! Add gcroot intrinsic.
						
					}
				}
			}
		}
	}
}