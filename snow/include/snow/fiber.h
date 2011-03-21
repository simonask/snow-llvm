#pragma once
#ifndef CONTINUATION_H_6NGEZ11
#define CONTINUATION_H_6NGEZ11

#include "snow/object.h"

struct SnCallFrame;

static const size_t SN_CONTINUATION_STACK_SIZE = 2*SN_MEMORY_PAGE_SIZE;

typedef enum SnFiberFlags {
	SnFiberNoFlags   = 0x0,
	SnFiberIsRunning = 0x1, // the opposite of suspended
	SnFiberIsStarted = 0x2, // started, but possibly suspended
} SnFiberFlags;


struct SnFiberState;

typedef struct SnFiber {
	SnObjectBase base;
	VALUE functor;
	VALUE incoming_value;
	struct SnFiber* link;
	struct SnFiberState* state;
} SnFiber;

CAPI SnFiber* snow_create_fiber(VALUE functor); // functor is called with arguments calling_fiber, incoming_value
CAPI VALUE snow_fiber_resume(SnFiber* fiber, VALUE incoming_value);
CAPI SnFiber* snow_get_current_fiber();
CAPI struct SnCallFrame* snow_fiber_get_current_frame(const SnFiber* fiber);

// Internal
CAPI void snow_fiber_begin_thread();
CAPI void snow_fiber_end_thread();

#endif /* end of include guard: CONTINUATION_H_6NGEZ11 */
