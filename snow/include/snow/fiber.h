#pragma once
#ifndef CONTINUATION_H_6NGEZ11
#define CONTINUATION_H_6NGEZ11

#include "snow/object.h"

struct SnCallFrame;
struct SnClass;
struct SnFiber;

// XXX: Really big for now, because LLVM lazy compilation requires a *lot* of stack space.
// TODO: Consider asynchonous compilation thread, so fibers and threads don't have to worry about that.
static const size_t SN_FIBER_STACK_SIZE = 8*SN_MEMORY_PAGE_SIZE; 

typedef VALUE(*SnFiberStartFunc)(struct SnFiber* fiber, struct SnFiber* caller, VALUE data);
typedef void(*SnFiberReturnFunc)(struct SnFiber* return_from, VALUE data);
typedef void(*SnStartFiberFunc)(struct SnFiber* fiber, struct SnFiber* caller, VALUE data, SnFiberStartFunc start_func, SnFiberReturnFunc return_callback);

typedef enum SnFiberFlags {
	SnFiberNoFlags   = 0x0,
	SnFiberIsRunning = 0x1, // the opposite of suspended
	SnFiberIsStarted = 0x2, // started, but possibly suspended
} SnFiberFlags;

struct SnFiberState;

typedef struct SnFiber {
	SnObject base;
	VALUE functor;
	VALUE incoming_value;
	byte* stack;
	byte* suspended_stack_boundary;
	struct SnFiber* link;
	struct SnFiberState* state;
	struct SnCallFrame* current_frame;
} SnFiber;

CAPI SnFiber* snow_create_fiber(VALUE functor); // functor is called with arguments calling_fiber, incoming_value
CAPI VALUE snow_fiber_resume(SnFiber* fiber, VALUE incoming_value);
CAPI SnFiber* snow_get_current_fiber();
CAPI struct SnCallFrame* snow_fiber_get_current_frame(const SnFiber* fiber);
CAPI struct SnClass* snow_get_fiber_class();

// Internal
CAPI void snow_fiber_begin_thread();
CAPI void snow_fiber_end_thread();
CAPI void snow_fiber_suspend_for_garbage_collection(SnFiber* fiber);

#endif /* end of include guard: CONTINUATION_H_6NGEZ11 */
