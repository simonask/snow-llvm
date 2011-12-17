#include "snow/fiber.h"
#include "fiber-internal.h"
#include "internal.h"
#include "util.hpp"
#include "snow/boolean.h"
#include "snow/class.h"
#include "snow/function.h"
#include "snow/gc.h"
#include "snow/snow.h"
#include "snow/str.h"
#include "snow/vm.h"
#include "snow/exception.h"

#include <pthread.h>
#include <sys/mman.h>
#include <setjmp.h>

namespace {
	struct FiberPrivate {
		VALUE functor;
		VALUE incoming_value;
		byte* stack;
		byte* suspended_stack_boundary;
		SnObject* link;
		SnCallFrame* current_frame;
		jmp_buf context;
		unsigned flags;
		
		FiberPrivate() :
			functor(NULL),
			incoming_value(NULL),
			stack(NULL),
			suspended_stack_boundary(NULL),
			link(NULL),
			current_frame(NULL),
			flags(SnFiberNoFlags)
		{
		}
		~FiberPrivate() {
			if (stack)
				munmap(stack, SN_FIBER_STACK_SIZE);
		}
	};
	
	void fiber_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		// TODO: Scan stack
	}
}

CAPI SnInternalType SnFiberType = {
	.data_size = sizeof(FiberPrivate),
	.initialize = snow::construct<FiberPrivate>,
	.finalize = snow::destruct<FiberPrivate>,
	.copy = NULL,
	.gc_each_root = fiber_gc_each_root,
};

static pthread_key_t _current_fiber;

static VALUE fiber_start(SnObject* fiber, SnObject* caller, VALUE data);
static void fiber_return(SnObject* return_from, VALUE data);
void snow_start_fiber(SnObject* fiber, SnObject* caller, VALUE data, SnFiberStartFunc start_func, SnFiberReturnFunc return_callback);

CAPI SnObject* snow_get_current_fiber() {
	SnObject** root = (SnObject**)pthread_getspecific(_current_fiber);
	return *root;
}

static void set_current_fiber(SnObject* fiber) {
	SnObject** root = (SnObject**)pthread_getspecific(_current_fiber);
	*root = fiber;
}

CAPI void snow_init_fibers() {
	pthread_key_create(&_current_fiber, NULL);
	snow_fiber_begin_thread(); // init main thread
}

CAPI void snow_fiber_begin_thread() {
	SnObject** current_fiber_ptr = (SnObject**)pthread_getspecific(_current_fiber);
	ASSERT(current_fiber_ptr == NULL);
	current_fiber_ptr = snow_gc_create_root(NULL);
	pthread_setspecific(_current_fiber, current_fiber_ptr);
	
	SnObject* fiber = snow_create_fiber(NULL);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	priv->flags = SnFiberIsRunning | SnFiberIsStarted;
	*current_fiber_ptr = fiber;
}

CAPI void snow_fiber_end_thread() {
	SnObject** root = (SnObject**)pthread_getspecific(_current_fiber);
	ASSERT(root != NULL);
	ASSERT(*root != NULL);
	snow_gc_free_root(root);
	pthread_setspecific(_current_fiber, NULL);
}

CAPI SnObject* snow_create_fiber(VALUE functor) {
	SnObject* fiber = snow_create_object_without_initialize(snow_get_fiber_class());
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv); // fiber is not a Fiber
	priv->functor = functor;
	return fiber;
}

static NO_INLINE byte* get_sp() {
	void* foo = NULL;
	byte* ptr = (byte*)&foo;
	return ptr;
}

static VALUE fiber_resume_internal(SnObject* fiber, VALUE incoming_value, bool update_link) {
	SnObject* caller = snow_get_current_fiber();
	if (caller == fiber) return incoming_value;
	
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv); // fiber is not a Fiber
	FiberPrivate* caller_priv = snow::object_get_private<FiberPrivate>(caller, SnFiberType);
	ASSERT(caller_priv); // caller is not a Fiber
	
	ASSERT(!(priv->flags & SnFiberIsRunning)); // tried to resume an already resumed fiber
	
	volatile bool came_back = false;
	
	SN_GC_WRLOCK(caller);
	caller_priv->flags &= ~SnFiberIsRunning;
	jmp_buf* caller_context = &caller_priv->context;
	caller_priv->suspended_stack_boundary = get_sp();
	setjmp(*caller_context);
	
	if (came_back) {
		SN_GC_WRLOCK(caller);
		caller_priv->flags |= SnFiberIsRunning;
		VALUE val = caller_priv->incoming_value;
		SN_GC_UNLOCK(caller);
		return val;
	} else {
		came_back = true;
		SN_GC_UNLOCK(caller);

		SN_GC_WRLOCK(fiber);
		if (update_link && priv->stack)
			priv->link = caller;
			
		if (priv->flags & SnFiberIsStarted) {
			priv->incoming_value = incoming_value;
			jmp_buf* fiber_context = &priv->context;
			set_current_fiber(fiber);
			SN_GC_UNLOCK(fiber);
			longjmp(*fiber_context, 1);
		} else {
			priv->flags |= SnFiberIsStarted;
			set_current_fiber(fiber);
			// SN_GC_UNLOCK is called by fiber_start callback.
			// (necessary because the platform-specific fiber starters need access
			// to the contents of the SnFiber object, but don't have access
			// to the locking mechanism.)
			snow_start_fiber(fiber, priv->stack, caller, incoming_value, fiber_start, fiber_return);
		}
		ASSERT(false); // unreachable
		return NULL;
	}
}

CAPI VALUE snow_fiber_resume(SnObject* fiber, VALUE incoming_value) {
	return fiber_resume_internal(fiber, incoming_value, true);
}

static VALUE fiber_start(SnObject* fiber, SnObject* caller, VALUE incoming_value) {
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv);
	VALUE functor = priv->functor;
	// fiber is locked by snow_fiber_resume
	SN_GC_UNLOCK(fiber);
	
	VALUE args[] = { (VALUE)caller, incoming_value };
	VALUE return_value = snow_call(functor, NULL, 2, args);
	
	return return_value;
}

static void fiber_return(SnObject* return_from, VALUE returned_value) {
	SN_GC_WRLOCK(return_from);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(return_from, SnFiberType);
	priv->flags &= ~SnFiberIsStarted;
	SnObject* link = priv->link;
	SN_GC_UNLOCK(return_from);
	fiber_resume_internal(link, returned_value, false);
	ASSERT(false);
}

CAPI SnCallFrame* snow_fiber_get_current_frame(const SnObject* fiber) {
	const FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv);
	return priv->current_frame;
}

CAPI SnObject* snow_fiber_get_link(const SnObject* fiber) {
	const FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv);
	return priv->link;
}

CAPI void snow_fiber_suspend_for_garbage_collection(SnObject* fiber) {
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv);
	priv->suspended_stack_boundary = get_sp();
	setjmp(priv->context); // bogus, never longjmp'ed to
}

//----------------------------------------------------------------------------

CAPI void snow_push_call_frame(SnCallFrame* frame) {
	SnObject* fiber = snow_get_current_fiber();
	ASSERT(fiber);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv);
	frame->caller = priv->current_frame;
	priv->current_frame = frame;
}

CAPI void snow_pop_call_frame(SnCallFrame* frame) {
	SnObject* fiber = snow_get_current_fiber();
	ASSERT(fiber);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>(fiber, SnFiberType);
	ASSERT(priv);
	ASSERT(priv->current_frame == frame);
	priv->current_frame = priv->current_frame->caller;
	frame->caller = NULL;
}

//----------------------------------------------------------------------------

static VALUE fiber_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, inspect);
	SnObject* c = (SnObject*)self;
	
	SN_GC_RDLOCK(c);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>((SnObject*)self, SnFiberType);
	VALUE functor = priv->functor;
	void* stack = priv->stack;
	SN_GC_UNLOCK(c);
	
	SnObject* inspected_functor = snow_value_inspect(functor);
	SnObject* result = snow_string_format("[Fiber@%p stack:%p functor:", c, stack);
	snow_string_append(result, inspected_functor);
	snow_string_append_cstr(result, "]");
	return result;
}

static VALUE fiber_resume(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, resume);
	SnObject* c = (SnObject*)self;
	return snow_fiber_resume(c, it);
}

static VALUE fiber_is_running(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, running);
	SN_GC_RDLOCK(self);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>((SnObject*)self, SnFiberType);
	bool is_running = (priv->flags & SnFiberIsRunning) != 0;
	SN_GC_UNLOCK(self);
	return snow_boolean_to_value(is_running);
}

static bool _fiber_is_started(const SnObject* fiber) {
	SN_GC_RDLOCK(fiber);
	FiberPrivate* priv = snow::object_get_private<FiberPrivate>((SnObject*)fiber, SnFiberType);
	bool is_started = (priv->flags & SnFiberIsStarted) != 0;
	SN_GC_UNLOCK(fiber);
	return is_started;
}

static VALUE fiber_is_started(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, is_started);
	SnObject* c = (SnObject*)self;
	return snow_boolean_to_value(_fiber_is_started(c));
}


static VALUE fiber_each(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, each);
	SnObject* f = (SnObject*)self;
	bool is_started = _fiber_is_started(f);
	VALUE first_result = snow_fiber_resume(f, NULL);
	snow_call(it, NULL, 1, &first_result);
	while(_fiber_is_started(f)) {
		VALUE result = snow_fiber_resume(f, NULL);
		snow_call(it, NULL, 1, &result);
	}
	return SN_NIL;
}

static VALUE fiber_initialize(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, initialize);
	FiberPrivate* priv = snow::value_get_private<FiberPrivate>(self, SnFiberType);
	priv->functor = it;
	return self;
}

CAPI SnObject* snow_get_fiber_class() {
	static SnObject** root = NULL;
	if (!root) {
		SnObject* cls = snow_create_class_for_type(snow_sym("Fiber"), &SnFiberType);
		snow_class_define_method(cls, "initialize", fiber_initialize);
		snow_class_define_method(cls, "inspect", fiber_inspect);
		snow_class_define_method(cls, "to_string", fiber_inspect);
		snow_class_define_method(cls, "resume", fiber_resume);
		snow_class_define_method(cls, "__call__", fiber_resume);
		snow_class_define_method(cls, "each", fiber_each);
		snow_class_define_property(cls, "running?", fiber_is_running, NULL);
		snow_class_define_property(cls, "started?", fiber_is_started, NULL);
		root = snow_gc_create_root(cls);
	}
	return *root;
}
