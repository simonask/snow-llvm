#pragma once
#ifndef VM_HPP_16FWWAY5
#define VM_HPP_16FWWAY5

#include "basic.hpp"

#include "snow/vm.h"

#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/DerivedTypes.h>

namespace snow {
	/*
	 * Installs the VM callbacks, and initializes the engine.
	 */
	void init_vm(SnVM* vm, llvm::ExecutionEngine* ee);
	
	llvm::FunctionType* get_function_type();
}

#endif /* end of include guard: VM_HPP_16FWWAY5 */
