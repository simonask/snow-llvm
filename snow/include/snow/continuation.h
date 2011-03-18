#pragma once
#ifndef CONTINUATION_H_6NGEZ11
#define CONTINUATION_H_6NGEZ11

#include "snow/object.h"

static const size_t SN_CONTINUATION_STACK_SIZE = 2*SN_MEMORY_PAGE_SIZE;

typedef enum SnContinuationFlags {
	SnContinuationNoFlags   = 0x0,
	SnContinuationIsRunning = 0x1, // the opposite of suspended
	SnContinuationIsStarted = 0x2, // started, but possibly suspended
} SnContinuationFlags;

typedef struct SnContinuation {
	SnObjectBase base;
	
	VALUE functor;
	VALUE incoming_value;
	struct SnContinuation* link;
	
	void* context;
	byte* stack;
	byte* suspended_stack_boundary;
	
	unsigned flags;
} SnContinuation;

CAPI SnContinuation* snow_create_continuation(VALUE functor);
CAPI VALUE snow_continuation_resume(SnContinuation* continuation, VALUE incoming_value);
CAPI SnContinuation* snow_get_current_continuation();

// Internal
CAPI void snow_continuation_begin_thread();
CAPI void snow_continuation_end_thread();

#endif /* end of include guard: CONTINUATION_H_6NGEZ11 */
