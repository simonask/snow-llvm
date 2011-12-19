#pragma once
#ifndef FIBER_INTERNAL_H_J24Y85C2
#define FIBER_INTERNAL_H_J24Y85C2

namespace snow {
	struct CallFrame;
}

struct SnObject;

// XXX: Really big for now, because LLVM lazy compilation requires a *lot* of stack space.
// TODO: Consider asynchonous compilation thread, so fibers and threads don't have to worry about that.
static const size_t SN_FIBER_STACK_SIZE = 8*SN_MEMORY_PAGE_SIZE;

typedef VALUE(*SnFiberStartFunc)(struct SnObject* fiber, struct SnObject* caller, VALUE data);
typedef void(*SnFiberReturnFunc)(struct SnObject* return_from, VALUE data);
typedef void(*SnStartFiberFunc)(struct SnObject* fiber, struct SnObject* caller, VALUE data, SnFiberStartFunc start_func, SnFiberReturnFunc return_callback);


CAPI void __attribute__((noreturn))
	snow_start_fiber(struct SnObject* fiber, byte* stack, struct SnObject* caller, VALUE data, SnFiberStartFunc start_func, SnFiberReturnFunc return_callback);

typedef enum SnFiberFlags {
	SnFiberNoFlags   = 0x0,
	SnFiberIsRunning = 0x1, // the opposite of suspended
	SnFiberIsStarted = 0x2, // started, but possibly suspended
} SnFiberFlags;

CAPI void snow_fiber_begin_thread();
CAPI void snow_fiber_end_thread();
CAPI void snow_fiber_suspend_for_garbage_collection(SnObject* fiber);
CAPI void snow_push_call_frame(struct snow::CallFrame* frame);
CAPI void snow_pop_call_frame(struct snow::CallFrame* frame);

#endif /* end of include guard: FIBER_INTERNAL_H_J24Y85C2 */
