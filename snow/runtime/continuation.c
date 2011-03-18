#include "snow/continuation.h"
#include <pthread.h>
#include <sys/mman.h>
#include <setjmp.h>
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/process.h"
#include "snow/vm.h"
#include "snow/boolean.h"
#include "snow/str.h"
#include "snow/function.h"

static pthread_key_t _current_continuation;

static VALUE continuation_start(SnContinuation* continuation, SnContinuation* caller, VALUE data);
static void continuation_return(SnContinuation* return_from, VALUE data);

SnContinuation* snow_get_current_continuation() {
	return (SnContinuation*)pthread_getspecific(_current_continuation);
}

static void set_current_continuation(SnContinuation* continuation) {
	pthread_setspecific(_current_continuation, continuation);
}

void snow_init_continuations() {
	pthread_key_create(&_current_continuation, NULL);
	snow_continuation_begin_thread();
}

void snow_finalize_continuation(SnContinuation* continuation) {
	free(continuation->context);
	if (continuation->stack) {
		munmap(continuation->stack, SN_CONTINUATION_STACK_SIZE);
	}
}

void snow_continuation_begin_thread() {
	ASSERT(snow_get_current_continuation() == NULL);
	SnContinuation* cc = SN_GC_ALLOC_FIXED(SnContinuation);
	SN_GC_WRLOCK(cc);
	cc->functor = NULL;
	cc->incoming_value = NULL;
	cc->link = NULL;
	cc->context = malloc(sizeof(jmp_buf));
	cc->stack = NULL;
	cc->suspended_stack_boundary = NULL;
	cc->flags = SnContinuationIsRunning | SnContinuationIsStarted;
	SN_GC_UNLOCK(cc);
	set_current_continuation(cc);
}

void snow_continuation_end_thread() {
	SnContinuation* cc = snow_get_current_continuation();
	ASSERT(cc != NULL);
	SN_GC_WRLOCK(cc);
	snow_finalize_continuation(cc);
	SN_GC_UNLOCK(cc);
	SN_GC_FREE_FIXED(cc);
}

SnContinuation* snow_create_continuation(VALUE functor) {
	SnContinuation* cc = SN_GC_ALLOC_OBJECT(SnContinuation);
	SN_GC_WRLOCK(cc);
	cc->functor = functor;
	cc->incoming_value = NULL;
	cc->link = NULL;
	cc->context = malloc(sizeof(jmp_buf));
	cc->stack = (byte*)mmap(NULL, SN_CONTINUATION_STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	cc->suspended_stack_boundary = NULL;
	cc->flags = SnContinuationNoFlags;
	SN_GC_UNLOCK(cc);
	return cc;
}

VALUE snow_continuation_resume(SnContinuation* continuation, VALUE incoming_value) {
	SnContinuation* caller = snow_get_current_continuation();
	if (caller == continuation) return incoming_value;
	
	ASSERT(!(continuation->flags & SnContinuationIsRunning));
	
	volatile bool came_back = false;
	
	SN_GC_WRLOCK(caller);
	caller->flags &= ~SnContinuationIsRunning;
	void* caller_context = caller->context;
	SN_GC_UNLOCK(caller);
	
	setjmp(caller_context);
	
	if (came_back) {
		set_current_continuation(caller);
		SN_GC_WRLOCK(caller);
		caller->flags |= SnContinuationIsRunning;
		VALUE val = caller->incoming_value;
		SN_GC_UNLOCK(caller);
		return val;
	} else {
		came_back = true;
		SN_GC_WRLOCK(continuation);
		continuation->link = caller;
		if (continuation->flags & SnContinuationIsStarted) {
			continuation->incoming_value = incoming_value;
			void* continuation_context = continuation->context;
			SN_GC_UNLOCK(continuation);
			longjmp(continuation_context, 1);
		} else {
			continuation->flags |= SnContinuationIsStarted;
			// SN_GC_UNLOCK is called by continuation_start callback.
			// (necessary because the platform-specific continuation starters need access
			// to the contents of the SnContinuation object, but don't have access
			// to the locking mechanism.)
			snow_get_process()->vm->start_continuation(continuation, caller, incoming_value, continuation_start, continuation_return);
		}
		ASSERT(false); // unreachable
		return NULL;
	}
}

static VALUE continuation_start(SnContinuation* continuation, SnContinuation* caller, VALUE incoming_value) {
	// continuation is locked by snow_continuation_resume
	VALUE functor = continuation->functor;
	SN_GC_UNLOCK(continuation);
	
	VALUE args[] = { (VALUE)caller, incoming_value };
	VALUE return_value = snow_call(functor, NULL, 2, args);
	
	return return_value;
}

static void continuation_return(SnContinuation* return_from, VALUE returned_value) {
	printf("returning from continuation %p with value %p\n", return_from, returned_value);
	SN_GC_WRLOCK(return_from);
	return_from->flags &= ~SnContinuationIsStarted;
	SN_GC_UNLOCK(return_from);
	snow_continuation_resume(return_from->link, returned_value);
}

//----------------------------------------------------------------------------

static VALUE continuation_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnContinuationType) return NULL;
	SnContinuation* c = (SnContinuation*)self;
	
	SN_GC_RDLOCK(c);
	SnContinuation safe_c;
	memcpy(&safe_c, c, sizeof(SnContinuation));
	SN_GC_UNLOCK(c);
	
	SnString* inspected_functor = snow_value_inspect(safe_c.functor);
	SnString* result = snow_string_format("[Continuation@%p stack:%p functor:", c, safe_c.stack);
	snow_string_append(result, inspected_functor);
	snow_string_append_cstr(result, "]");
	return result;
}

static VALUE continuation_resume(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnContinuationType) return NULL;
	SnContinuation* c = (SnContinuation*)self;
	return snow_continuation_resume(c, it);
}

static VALUE continuation_is_running(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnContinuationType) return NULL;
	SnContinuation* c = (SnContinuation*)self;
	SN_GC_RDLOCK(c);
	bool is_running = (c->flags & SnContinuationIsRunning) != 0;
	SN_GC_UNLOCK(c);
	return snow_boolean_to_value(is_running);
}

static VALUE continuation_is_started(SnFunctionCallContext* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnContinuationType) return NULL;
	SnContinuation* c = (SnContinuation*)self;
	SN_GC_RDLOCK(c);
	bool is_started = (c->flags & SnContinuationIsStarted) != 0;
	SN_GC_UNLOCK(c);
	return snow_boolean_to_value(is_started);
}

SnObject* snow_create_continuation_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", continuation_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", continuation_inspect, 0);
	SN_DEFINE_METHOD(proto, "resume", continuation_resume, 1);
	SN_DEFINE_PROPERTY(proto, "running?", continuation_is_running, NULL);
	SN_DEFINE_PROPERTY(proto, "started?", continuation_is_started, NULL);
	return proto;
}
