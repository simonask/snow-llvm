#include "snow/fiber.h"
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

static pthread_key_t _current_fiber;

static VALUE fiber_start(SnFiber* fiber, SnFiber* caller, VALUE data);
static void fiber_return(SnFiber* return_from, VALUE data);

SnFiber* snow_get_current_fiber() {
	return (SnFiber*)pthread_getspecific(_current_fiber);
}

static void set_current_fiber(SnFiber* fiber) {
	pthread_setspecific(_current_fiber, fiber);
}

void snow_init_fibers() {
	pthread_key_create(&_current_fiber, NULL);
	snow_fiber_begin_thread();
}

void snow_finalize_fiber(SnFiber* fiber) {
	free(fiber->context);
	if (fiber->stack) {
		munmap(fiber->stack, SN_CONTINUATION_STACK_SIZE);
	}
}

void snow_fiber_begin_thread() {
	ASSERT(snow_get_current_fiber() == NULL);
	SnFiber* cc = SN_GC_ALLOC_FIXED(SnFiber);
	SN_GC_WRLOCK(cc);
	cc->functor = NULL;
	cc->incoming_value = NULL;
	cc->link = NULL;
	cc->context = malloc(sizeof(jmp_buf));
	cc->stack = NULL;
	cc->suspended_stack_boundary = NULL;
	cc->flags = SnFiberIsRunning | SnFiberIsStarted;
	SN_GC_UNLOCK(cc);
	set_current_fiber(cc);
}

void snow_fiber_end_thread() {
	SnFiber* cc = snow_get_current_fiber();
	ASSERT(cc != NULL);
	SN_GC_WRLOCK(cc);
	snow_finalize_fiber(cc);
	SN_GC_UNLOCK(cc);
	SN_GC_FREE_FIXED(cc);
}

SnFiber* snow_create_fiber(VALUE functor) {
	SnFiber* cc = SN_GC_ALLOC_OBJECT(SnFiber);
	SN_GC_WRLOCK(cc);
	cc->functor = functor;
	cc->incoming_value = NULL;
	cc->link = NULL;
	cc->context = malloc(sizeof(jmp_buf));
	cc->stack = (byte*)mmap(NULL, SN_CONTINUATION_STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	cc->suspended_stack_boundary = NULL;
	cc->flags = SnFiberNoFlags;
	SN_GC_UNLOCK(cc);
	return cc;
}

static NO_INLINE byte* get_sp() {
	void* foo = NULL;
	byte* ptr = (byte*)&foo;
	return ptr;
}

static VALUE fiber_resume_internal(SnFiber* fiber, VALUE incoming_value, bool update_link) {
	SnFiber* caller = snow_get_current_fiber();
	if (caller == fiber) return incoming_value;
	
	ASSERT(!(fiber->flags & SnFiberIsRunning));
	
	volatile bool came_back = false;
	
	SN_GC_WRLOCK(caller);
	caller->flags &= ~SnFiberIsRunning;
	void* caller_context = caller->context;
	caller->suspended_stack_boundary = get_sp();
	setjmp(caller_context);
	
	if (came_back) {
		SN_GC_WRLOCK(caller);
		caller->flags |= SnFiberIsRunning;
		VALUE val = caller->incoming_value;
		SN_GC_UNLOCK(caller);
		return val;
	} else {
		came_back = true;
		SN_GC_UNLOCK(caller);

		SN_GC_WRLOCK(fiber);
		if (update_link)
			fiber->link = caller;
			
		if (fiber->flags & SnFiberIsStarted) {
			fiber->incoming_value = incoming_value;
			void* fiber_context = fiber->context;
			SN_GC_UNLOCK(fiber);
			set_current_fiber(fiber);
			longjmp(fiber_context, 1);
		} else {
			fiber->flags |= SnFiberIsStarted;
			set_current_fiber(fiber);
			// SN_GC_UNLOCK is called by fiber_start callback.
			// (necessary because the platform-specific fiber starters need access
			// to the contents of the SnFiber object, but don't have access
			// to the locking mechanism.)
			snow_get_process()->vm->start_fiber(fiber, caller, incoming_value, fiber_start, fiber_return);
		}
		ASSERT(false); // unreachable
		return NULL;
	}
}


VALUE snow_fiber_resume(SnFiber* fiber, VALUE incoming_value) {
	return fiber_resume_internal(fiber, incoming_value, true);
}

static VALUE fiber_start(SnFiber* fiber, SnFiber* caller, VALUE incoming_value) {
	// fiber is locked by snow_fiber_resume
	VALUE functor = fiber->functor;
	SN_GC_UNLOCK(fiber);
	
	VALUE args[] = { (VALUE)caller, incoming_value };
	VALUE return_value = snow_call(functor, NULL, 2, args);
	
	return return_value;
}

static void fiber_return(SnFiber* return_from, VALUE returned_value) {
	SN_GC_WRLOCK(return_from);
	return_from->flags &= ~SnFiberIsStarted;
	SnFiber* link = return_from->link;
	SN_GC_UNLOCK(return_from);
	ASSERT(false);
	fiber_resume_internal(link, returned_value, false);
}

//----------------------------------------------------------------------------

static VALUE fiber_inspect(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	
	SN_GC_RDLOCK(c);
	SnFiber safe_c;
	memcpy(&safe_c, c, sizeof(SnFiber));
	SN_GC_UNLOCK(c);
	
	SnString* inspected_functor = snow_value_inspect(safe_c.functor);
	SnString* result = snow_string_format("[Fiber@%p stack:%p functor:", c, safe_c.stack);
	snow_string_append(result, inspected_functor);
	snow_string_append_cstr(result, "]");
	return result;
}

static VALUE fiber_resume(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	return snow_fiber_resume(c, it);
}

static VALUE fiber_is_running(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	SN_GC_RDLOCK(c);
	bool is_running = (c->flags & SnFiberIsRunning) != 0;
	SN_GC_UNLOCK(c);
	return snow_boolean_to_value(is_running);
}

static bool _fiber_is_started(const SnFiber* fiber) {
	SN_GC_RDLOCK(fiber);
	bool is_started = (fiber->flags & SnFiberIsStarted) != 0;
	SN_GC_UNLOCK(fiber);
	return is_started;
}

static VALUE fiber_is_started(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	return snow_boolean_to_value(_fiber_is_started(c));
}


static VALUE fiber_each(SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* f = (SnFiber*)self;
	bool is_started = _fiber_is_started(f);
	VALUE first_result = snow_fiber_resume(f, NULL);
	snow_call(it, NULL, 1, &first_result);
	while(_fiber_is_started(f)) {
		VALUE result = snow_fiber_resume(f, NULL);
		snow_call(it, NULL, 1, &result);
	}
	return SN_NIL;
}

SnObject* snow_create_fiber_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", fiber_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", fiber_inspect, 0);
	SN_DEFINE_METHOD(proto, "resume", fiber_resume, 1);
	SN_DEFINE_METHOD(proto, "each", fiber_each, 1);
	SN_DEFINE_PROPERTY(proto, "running?", fiber_is_running, NULL);
	SN_DEFINE_PROPERTY(proto, "started?", fiber_is_started, NULL);
	return proto;
}
