#include "snow/fiber.h"
#include "snow/vm.h"

#include <assert.h>

static void _fiber_trampoline() {
	__asm__ __volatile__(
		"movq 0x10(%%rsp), %%rdi\n"
		"movq %%rax, %%rsi\n"     // value returned from the 
		"movq 0x8(%%rsp), %%r8\n" // return callback
		"jmp *%%r8\n"
	: : : "%rdi", "%rsi", "%r8"
	);
}

void __attribute__((noreturn))
libsnow_fiber_start(SnFiber* fiber, SnFiber* caller, VALUE data, SnFiberStartFunc start_func, SnFiberReturnFunc return_callback)
{
	void** stack_top = (void**)(fiber->stack + SN_FIBER_STACK_SIZE);
	
	*(stack_top-1) = fiber;
	*(stack_top-2) = (void*)return_callback; 
	*(stack_top-3) = (void*)_fiber_trampoline; // "saved" rip
	
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
		: "r"(fiber), "r"(caller), "r"(data), "r"(start_func), "r"(stack_top-3)
		: "%rdi", "%rsi", "%rdx", "%r8", "%rsp"
	);
	
	assert(false);
}