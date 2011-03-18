#include "snow/continuation.h"
#include "snow/vm.h"

#include <assert.h>

static void _continuation_trampoline() {
	__asm__ __volatile__(
		"movq 0x10(%%rsp), %%rdi\n"
		"movq %%rax, %%rsi\n"     // value returned from the 
		"movq 0x8(%%rsp), %%r8\n" // return callback
		"call *%%r8\n"
	: : : "%rdi", "%rsi", "%r8"
	);
}

void __attribute__((noreturn))
libsnow_continuation_start(SnContinuation* continuation, SnContinuation* caller, VALUE data, SnContinuationStartFunc start_func, SnContinuationReturnFunc return_callback)
{
	void** stack_top = (void**)(continuation->stack + SN_CONTINUATION_STACK_SIZE);
	
	*(stack_top-1) = continuation;
	*(stack_top-2) = (void*)return_callback; 
	*(stack_top-3) = (void*)_continuation_trampoline; // "saved" rip
	
	__asm__ __volatile__(
		"movq %0, %%rdi\n"
		"movq %1, %%rsi\n"
		"movq %2, %%rdx\n"
		"movq %3, %%r8\n"
		"movq %4, %%rsp\n"
		"movq %%rsp, %%rbp\n"
		"subq $0x10, %%rbp\n" // rbp for trampoline
		"jmpq *%%r8\n"
		:
		: "r"(continuation), "r"(caller), "r"(data), "r"(start_func), "r"(stack_top-3)
		: "%rdi", "%rsi", "%rdx", "%r8", "%rsp"
	);
	
	assert(false);
}