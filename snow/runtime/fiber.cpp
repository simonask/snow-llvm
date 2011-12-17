#include "snow/fiber.h"
#include "fiber-internal.h"
#include "internal.h"
#include "util.hpp"
#include "objectptr.hpp"
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

using namespace snow;

namespace {
	struct Fiber {
		static const SnInternalType* Type;
		
		VALUE functor;
		VALUE incoming_value;
		byte* stack;
		byte* suspended_stack_boundary;
		snow::ObjectPtr<Fiber> link;
		SnCallFrame* current_frame;
		jmp_buf context;
		unsigned flags;
		
		Fiber() :
			functor(NULL),
			incoming_value(NULL),
			stack(NULL),
			suspended_stack_boundary(NULL),
			current_frame(NULL),
			flags(SnFiberNoFlags)
		{
		}
		~Fiber() {
			if (stack)
				munmap(stack, SN_FIBER_STACK_SIZE);
		}
	};
	const SnInternalType* Fiber::Type = &SnFiberType;
	
	void fiber_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		// TODO: Scan stack
	}
}

CAPI SnInternalType SnFiberType = {
	.data_size = sizeof(Fiber),
	.initialize = snow::construct<Fiber>,
	.finalize = snow::destruct<Fiber>,
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
	
	ObjectPtr<Fiber> fiber = snow_create_fiber(NULL);
	fiber->flags = SnFiberIsRunning | SnFiberIsStarted;
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
	ObjectPtr<Fiber> fiber = snow_create_object_without_initialize(snow_get_fiber_class());
	fiber->functor = functor;
	return fiber;
}

static NO_INLINE byte* get_sp() {
	void* foo = NULL;
	byte* ptr = (byte*)&foo;
	return ptr;
}

static VALUE fiber_resume_internal(const ObjectPtr<Fiber>& fiber, VALUE incoming_value, bool update_link) {
	ObjectPtr<Fiber> caller = snow_get_current_fiber();
	if (caller == fiber) return incoming_value;
	
	ASSERT(fiber != NULL); // fiber is not a Fiber
	
	ASSERT(!(fiber->flags & SnFiberIsRunning)); // tried to resume an already resumed fiber
	
	volatile bool came_back = false;
	
	caller->flags &= ~SnFiberIsRunning;
	jmp_buf* caller_context = &caller->context;
	caller->suspended_stack_boundary = get_sp();
	setjmp(*caller_context);
	
	if (came_back) {
		caller->flags |= SnFiberIsRunning;
		VALUE val = caller->incoming_value;
		return val;
	} else {
		came_back = true;

		if (update_link && fiber->stack)
			fiber->link = caller;
			
		if (fiber->flags & SnFiberIsStarted) {
			fiber->incoming_value = incoming_value;
			jmp_buf* fiber_context = &fiber->context;
			set_current_fiber(fiber);
			longjmp(*fiber_context, 1);
		} else {
			fiber->flags |= SnFiberIsStarted;
			set_current_fiber(fiber);
			snow_start_fiber(fiber, fiber->stack, caller, incoming_value, fiber_start, fiber_return);
		}
		ASSERT(false); // unreachable
		return NULL;
	}
}

CAPI VALUE snow_fiber_resume(SnObject* fiber, VALUE incoming_value) {
	return fiber_resume_internal(fiber, incoming_value, true);
}

static VALUE fiber_start(SnObject* fiber_, SnObject* caller, VALUE incoming_value) {
	VALUE functor = ObjectPtr<Fiber>(fiber_)->functor;
	
	VALUE args[] = { (VALUE)caller, incoming_value };
	VALUE return_value = snow_call(functor, NULL, 2, args);
	
	return return_value;
}

static void fiber_return(SnObject* return_from_, VALUE returned_value) {
	ObjectPtr<Fiber> return_from = return_from_;
	return_from->flags &= ~SnFiberIsStarted;
	ObjectPtr<Fiber> link = return_from->link;
	fiber_resume_internal(link, returned_value, false);
	ASSERT(false);
}

CAPI SnCallFrame* snow_fiber_get_current_frame(const SnObject* fiber) {
	return ObjectPtr<const Fiber>(fiber)->current_frame;
}

CAPI SnObject* snow_fiber_get_link(const SnObject* fiber) {
	return ObjectPtr<const Fiber>(fiber)->link;
}

CAPI void snow_fiber_suspend_for_garbage_collection(SnObject* fiber) {
	ObjectPtr<Fiber> f = fiber;
	f->suspended_stack_boundary = get_sp();
	setjmp(f->context); // bogus, never longjmp'ed to
}

//----------------------------------------------------------------------------

CAPI void snow_push_call_frame(SnCallFrame* frame) {
	ObjectPtr<Fiber> fiber = snow_get_current_fiber();
	frame->caller = fiber->current_frame;
	fiber->current_frame = frame;
}

CAPI void snow_pop_call_frame(SnCallFrame* frame) {
	ObjectPtr<Fiber> fiber = snow_get_current_fiber();
	ASSERT(fiber->current_frame == frame);
	fiber->current_frame = fiber->current_frame->caller;
	frame->caller = NULL;
}

//----------------------------------------------------------------------------

static VALUE fiber_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, inspect);
	ObjectPtr<Fiber> fiber = self;
	
	VALUE functor = fiber->functor;
	void* stack = fiber->stack;
	
	SnObject* inspected_functor = snow_value_inspect(functor);
	SnObject* result = snow_string_format("[Fiber@%p stack:%p functor:", (VALUE)fiber, stack);
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
	ObjectPtr<Fiber> fiber = self;
	bool is_running = (fiber->flags & SnFiberIsRunning) != 0;
	return snow_boolean_to_value(is_running);
}

static bool _fiber_is_started(const ObjectPtr<const Fiber>& fiber) {
	bool is_started = (fiber->flags & SnFiberIsStarted) != 0;
	return is_started;
}

static VALUE fiber_is_started(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Fiber, is_started);
	return snow_boolean_to_value(_fiber_is_started(self));
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
	ObjectPtr<Fiber> fiber = self;
	fiber->functor = it;
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
