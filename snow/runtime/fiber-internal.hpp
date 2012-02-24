#pragma once
#ifndef FIBER_INTERNAL_H_J24Y85C2
#define FIBER_INTERNAL_H_J24Y85C2

#include "snow/fiber.hpp"

struct SnObject;

namespace snow {
	struct CallFrame;
	
	static const size_t SN_FIBER_STACK_SIZE = 8*SN_MEMORY_PAGE_SIZE;

	typedef VALUE(*FiberStartFunc)(FiberPtr fiber, FiberPtr caller, VALUE data);
	typedef void(*FiberReturnFunc)(FiberPtr return_from, VALUE data);
	typedef void(*StartFiberFunc)(FiberPtr fiber, FiberPtr caller, VALUE data, FiberStartFunc start_func, FiberReturnFunc return_callback);

	void __attribute__((noreturn))
	start_fiber(FiberPtr fiber, byte* stack, FiberPtr caller, VALUE data, FiberStartFunc start_func, FiberReturnFunc return_callback);

	enum FiberFlags {
		FiberNoFlags   = 0x0,
		FiberIsRunning = 0x1, // the opposite of suspended
		FiberIsStarted = 0x2, // started, but possibly suspended
	};

	void fiber_begin_thread();
	void fiber_end_thread();
	void fiber_suspend_for_garbage_collection(FiberPtr fiber);
	void push_call_frame(CallFrame* frame);
	void pop_call_frame(CallFrame* frame);
}


#endif /* end of include guard: FIBER_INTERNAL_H_J24Y85C2 */
