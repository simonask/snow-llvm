#pragma once
#ifndef CCONV_HPP_8CIXGM8Q
#define CCONV_HPP_8CIXGM8Q

#include "asm.hpp"

namespace snow {
namespace x86_64 {
	// Define calling convention. TODO: Define for Win32 as well.
	static const int STACK_GROWTH          = -(int)sizeof(void*);
	static const Register REG_RETURN       = RAX;
	static const Register REG_ARGS[]       = { RDI, RSI, RDX, RCX, R8, R9 };
	static const Register REG_PRESERVE[]   = { RBX, R12, R13, R14, R15 };
	static const Register REG_CALL_FRAME   = R12;
	static const Register REG_METHOD_CACHE = R13;
	static const Register REG_IVAR_CACHE   = R14;
	static const Register REG_SCRATCH[]    = { R10, R11, RDI, RSI, RDX, RCX, R8, R9, RAX };
	static const Register REG_PRESERVED_SCRATCH[] = { RBX, R15 };
}
}

#endif /* end of include guard: CCONV_HPP_8CIXGM8Q */
