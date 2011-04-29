#include "snow/fiber.h"
#include "internal.h"
#include "snow/class.h"
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

typedef struct SnFiberState {
	// SnFiberState is necessary because SnFiber itself is subject
	// to the size requirements of regular Snow objects.
	jmp_buf context;
	unsigned flags;
} SnFiberState;

static pthread_key_t _current_fiber;

static VALUE fiber_start(SnFiber* fiber, SnFiber* caller, VALUE data);
static void fiber_return(SnFiber* return_from, VALUE data);

SnFiber* snow_get_current_fiber() {
	VALUE* root = (VALUE*)pthread_getspecific(_current_fiber);
	return (SnFiber*)*root;
}

static void set_current_fiber(SnFiber* fiber) {
	VALUE* root = (VALUE*)pthread_getspecific(_current_fiber);
	*root = fiber;
}

void snow_init_fibers() {
	pthread_key_create(&_current_fiber, NULL);
	snow_fiber_begin_thread(); // init main thread
}

void snow_finalize_fiber(SnFiber* fiber) {
	if (fiber->stack) {
		munmap(fiber->stack, SN_FIBER_STACK_SIZE);
	}
	free(fiber->state);
}

void snow_fiber_begin_thread() {
	VALUE* current_fiber_ptr = (VALUE*)pthread_getspecific(_current_fiber);
	ASSERT(current_fiber_ptr == NULL);
	current_fiber_ptr = snow_gc_create_root(NULL);
	pthread_setspecific(_current_fiber, current_fiber_ptr);
	
	ASSERT(snow_get_current_fiber() == NULL);
	SnFiber* fiber = SN_GC_ALLOC_OBJECT(SnFiber);
	SN_GC_WRLOCK(fiber);
	fiber->functor = NULL;
	fiber->incoming_value = NULL;
	fiber->link = NULL;
	fiber->state = (SnFiberState*)malloc(sizeof(SnFiberState));
	fiber->stack = NULL;
	fiber->suspended_stack_boundary = NULL;
	fiber->state->flags = SnFiberIsRunning | SnFiberIsStarted;
	fiber->current_frame = NULL;
	SN_GC_UNLOCK(fiber);
	*current_fiber_ptr = fiber;
}

void snow_fiber_end_thread() {
	VALUE* root = (VALUE*)pthread_getspecific(_current_fiber);
	ASSERT(root != NULL);
	ASSERT(*root != NULL);
	snow_gc_free_root(root);
	pthread_setspecific(_current_fiber, NULL);
}

SnFiber* snow_create_fiber(VALUE functor) {
	SnFiber* fiber = SN_GC_ALLOC_OBJECT(SnFiber);
	SN_GC_WRLOCK(fiber);
	fiber->functor = functor;
	fiber->incoming_value = NULL;
	fiber->link = NULL;
	fiber->state = (SnFiberState*)malloc(sizeof(SnFiberState));
	fiber->stack = (byte*)mmap(NULL, SN_FIBER_STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	fiber->suspended_stack_boundary = NULL;
	fiber->state->flags = SnFiberNoFlags;
	fiber->current_frame = NULL;
	SN_GC_UNLOCK(fiber);
	return fiber;
}

static NO_INLINE byte* get_sp() {
	void* foo = NULL;
	byte* ptr = (byte*)&foo;
	return ptr;
}

static VALUE fiber_resume_internal(SnFiber* fiber, VALUE incoming_value, bool update_link) {
	SnFiber* caller = snow_get_current_fiber();
	if (caller == fiber) return incoming_value;
	
	ASSERT(!(fiber->state->flags & SnFiberIsRunning));
	
	volatile bool came_back = false;
	
	SN_GC_WRLOCK(caller);
	caller->state->flags &= ~SnFiberIsRunning;
	jmp_buf* caller_context = &caller->state->context;
	caller->suspended_stack_boundary = get_sp();
	setjmp(*caller_context);
	
	if (came_back) {
		SN_GC_WRLOCK(caller);
		caller->state->flags |= SnFiberIsRunning;
		VALUE val = caller->incoming_value;
		SN_GC_UNLOCK(caller);
		return val;
	} else {
		came_back = true;
		SN_GC_UNLOCK(caller);

		SN_GC_WRLOCK(fiber);
		if (update_link && fiber->stack)
			fiber->link = caller;
			
		if (fiber->state->flags & SnFiberIsStarted) {
			fiber->incoming_value = incoming_value;
			jmp_buf* fiber_context = &fiber->state->context;
			set_current_fiber(fiber);
			SN_GC_UNLOCK(fiber);
			longjmp(*fiber_context, 1);
		} else {
			fiber->state->flags |= SnFiberIsStarted;
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
	VALUE functor = fiber->functor;
	// fiber is locked by snow_fiber_resume
	SN_GC_UNLOCK(fiber);
	
	VALUE args[] = { (VALUE)caller, incoming_value };
	VALUE return_value = snow_call(functor, NULL, 2, args);
	
	return return_value;
}

static void fiber_return(SnFiber* return_from, VALUE returned_value) {
	SN_GC_WRLOCK(return_from);
	return_from->state->flags &= ~SnFiberIsStarted;
	SnFiber* link = return_from->link;
	SN_GC_UNLOCK(return_from);
	fiber_resume_internal(link, returned_value, false);
	ASSERT(false);
}

SnCallFrame* snow_fiber_get_current_frame(const SnFiber* fiber) {
	SnCallFrame* frame = fiber->current_frame;
	return frame;
}

void snow_fiber_suspend_for_garbage_collection(SnFiber* fiber) {
	fiber->suspended_stack_boundary = get_sp();
	setjmp(fiber->state->context);
}

//----------------------------------------------------------------------------

// Called from codegen'ed functions to maintain the call chain

void _snow_push_call_frame(SnCallFrame* frame) {
	// TODO: Consider locking/allocation of SnCallFrame
	SnFiber* fiber = snow_get_current_fiber();
	frame->caller = fiber->current_frame;
	fiber->current_frame = frame;
}

void _snow_pop_call_frame(SnCallFrame* frame) {
	// TODO: Consider locking/allocation of SnCallFrame
	SnFiber* fiber = snow_get_current_fiber();
	ASSERT(fiber->current_frame == frame);
	fiber->current_frame = fiber->current_frame->caller;
	frame->caller = NULL;
}

//----------------------------------------------------------------------------

static VALUE fiber_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	
	SN_GC_RDLOCK(c);
	VALUE functor = c->functor;
	void* stack = c->stack;
	SN_GC_UNLOCK(c);
	
	SnString* inspected_functor = snow_value_inspect(functor);
	SnString* result = snow_string_format("[Fiber@%p stack:%p functor:", c, stack);
	snow_string_append(result, inspected_functor);
	snow_string_append_cstr(result, "]");
	return result;
}

static VALUE fiber_resume(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	return snow_fiber_resume(c, it);
}

static VALUE fiber_is_running(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	SN_GC_RDLOCK(c);
	bool is_running = (c->state->flags & SnFiberIsRunning) != 0;
	SN_GC_UNLOCK(c);
	return snow_boolean_to_value(is_running);
}

static bool _fiber_is_started(const SnFiber* fiber) {
	SN_GC_RDLOCK(fiber);
	bool is_started = (fiber->state->flags & SnFiberIsStarted) != 0;
	SN_GC_UNLOCK(fiber);
	return is_started;
}

static VALUE fiber_is_started(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	if (snow_type_of(self) != SnFiberType) return NULL;
	SnFiber* c = (SnFiber*)self;
	return snow_boolean_to_value(_fiber_is_started(c));
}


static VALUE fiber_each(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
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

SnClass* snow_get_fiber_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("inspect", fiber_inspect, 0),
			SN_METHOD("to_string", fiber_inspect, 0),
			SN_METHOD("resume", fiber_resume, 1),
			SN_METHOD("each", fiber_each, 1),
			SN_PROPERTY("running?", fiber_is_running, NULL),
			SN_PROPERTY("started?", fiber_is_started, NULL),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Fiber"), NULL, 0, NULL, countof(methods), methods);
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}
