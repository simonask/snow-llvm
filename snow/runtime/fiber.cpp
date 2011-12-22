#include "snow/fiber.hpp"
#include "fiber-internal.hpp"
#include "internal.h"
#include "semaphore.hpp"
#include "snow/util.hpp"
#include "snow/type.hpp"
#include "snow/objectptr.hpp"
#include "snow/boolean.hpp"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/gc.hpp"
#include "snow/snow.hpp"
#include "snow/str.hpp"
#include "snow/exception.hpp"

#include <thread>

namespace snow {
	struct Fiber {
		enum State {
			Running,
			Waiting,
			Terminating,
			Stopped,
		};
		struct Terminate {};
		
		Value functor;
		Value incoming_value;
		std::thread* thread;
		ObjectPtr<Fiber> resumed_by;
		Semaphore semaphore;
		byte* stack_top;
		byte* stack_bottom;
		CallFrame* current_frame;
		State state;

		Fiber() :
		functor(NULL),
			incoming_value(NULL),
			thread(NULL),
			stack_top(NULL),
			stack_bottom(NULL),
			current_frame(NULL),
			state(Stopped)
		{
		}
		
		void initialize(const Value& func) {
			functor = func;
			state = Waiting;
			thread = new std::thread(Fiber::_start, this);
		}
		
		void initialize_main(byte* system_stack_top) {
			functor = NULL;
			thread = NULL;
			state = Running;
			stack_top = system_stack_top;
		}
		
		~Fiber() {
			if (thread != NULL) {
				terminate();
				thread->join();
			}
		}
	private:
		void start();
		void terminate();
		static void _start(Fiber* f) { f->start(); }
	};

	SN_REGISTER_TYPE(Fiber, ((Type){ .data_size = sizeof(Fiber), .initialize = snow::construct<Fiber>, .finalize = snow::destruct<Fiber>, .copy = NULL, .gc_each_root = NULL}))

	static Value* _current_fiber = NULL;

	ObjectPtr<Fiber> get_current_fiber() {
		return *_current_fiber;
	}

	static void set_current_fiber(FiberPtr fiber) {
		*_current_fiber = (const Value&)fiber;
	}
	
	static NO_INLINE byte* get_sp() {
		void* foo = NULL;
		byte* ptr = (byte*)&foo;
		return ptr;
	}

	void init_fibers() {
		ObjectPtr<Fiber> fiber = create_object_without_initialize(get_fiber_class());
		_current_fiber = gc_create_root(fiber);
		fiber->initialize_main(get_sp());
	}

	ObjectPtr<Fiber> create_fiber(const Value& functor) {
		ObjectPtr<Fiber> fiber = create_object_without_initialize(get_fiber_class());
		fiber->initialize(functor);
		return fiber;
	}
	
	Value fiber_resume_internal(FiberPtr fiber, const Value& incoming_value, Fiber::State sleeping_state) {
		ObjectPtr<Fiber> current = get_current_fiber();
		if (fiber == current) return incoming_value;
		
		if (fiber->state == Fiber::Terminating)
			throw_exception_with_description("ERROR: Cannot resume terminating fiber.");
		
		ASSERT(fiber->state == Fiber::Waiting || fiber->state == Fiber::Stopped);
		fiber->incoming_value = incoming_value;
		fiber->state = Fiber::Running;
		fiber->resumed_by = current;
		current->state = sleeping_state;
		current->stack_bottom = get_sp();
		set_current_fiber(fiber);
		fiber->semaphore.signal();
		current->semaphore.wait();
		if (current->state == Fiber::Terminating) throw Fiber::Terminate();
		return current->incoming_value;
	}

	Value fiber_resume(FiberPtr fiber, const Value& incoming_value) {
		return fiber_resume_internal(fiber, incoming_value, Fiber::Waiting);
	}

	CallFrame* fiber_get_current_frame(FiberConstPtr fiber) {
		return fiber->current_frame;
	}

	ObjectPtr<Fiber> fiber_get_link(FiberConstPtr fiber) {
		return fiber->resumed_by;
	}
	
	void Fiber::start() {
		stack_top = get_sp();
		stack_bottom = stack_top;
		
		semaphore.wait();
		try {
			while (state != Terminating) {
				Value val = snow::call(functor, NULL, 2, (Value[]){ resumed_by, incoming_value });
				incoming_value = fiber_resume_internal(resumed_by, val, Stopped);
			}
		}
		catch (Fiber::Terminate t) {
			fiber_resume_internal(resumed_by, SN_NIL, Terminating);
		}
	}
	
	void Fiber::terminate() {
		ObjectPtr<Fiber> current = get_current_fiber();
		if (&*current == this)
			TRAP(); // terminating current fiber.
		state = Terminating;
		resumed_by = current;
		current->state = Waiting;
		
		semaphore.signal();
		current->semaphore.wait();
		ASSERT(current->state = Running);
	}

	void push_call_frame(CallFrame* frame) {
		ObjectPtr<Fiber> fiber = get_current_fiber();
		frame->caller = fiber->current_frame;
		fiber->current_frame = frame;
	}

	void pop_call_frame(CallFrame* frame) {
		ObjectPtr<Fiber> fiber = get_current_fiber();
		ASSERT(fiber->current_frame == frame);
		fiber->current_frame = fiber->current_frame->caller;
		frame->caller = NULL;
	}

	namespace bindings {
		static Value fiber_inspect(const CallFrame* here, const Value& self, const Value& it) {
			ObjectPtr<Fiber> fiber = self;

			const Value& functor = fiber->functor;

			ObjectPtr<String> inspected_functor = value_inspect(functor);
			ObjectPtr<String> result = string_format("[Fiber@%p stack:[%p-%p] functor:", fiber.value(), fiber->stack_bottom, fiber->stack_top);
			string_append(result, inspected_functor);
			string_append_cstr(result, "]");
			return result;
		}

		static Value fiber_resume(const CallFrame* here, const Value& self, const Value& it) {
			return snow::fiber_resume(self, it);
		}

		static Value fiber_is_running(const CallFrame* here, const Value& self, const Value& it) {
			ObjectPtr<Fiber> fiber = self;
			return boolean_to_value(fiber->state == Fiber::Running);
		}
		
		static Value fiber_is_started(const CallFrame* here, const Value& self, const Value& it) {
			return boolean_to_value(ObjectPtr<Fiber>(self)->state != Fiber::Stopped);
		}

		static Value fiber_each(const CallFrame* here, const Value& self, const Value& it) {
			ObjectPtr<Fiber> f = self;
			ASSERT(f != NULL);
			while (f->state == Fiber::Waiting) {
				Value result = fiber_resume(f, NULL);
				call(it, NULL, 1, &result);
			}
			return SN_NIL;
		}

		static Value fiber_initialize(const CallFrame* here, const Value& self, const Value& it) {
			ObjectPtr<Fiber> fiber = self;
			fiber->initialize(it);
			return self;
		}
	}

	ObjectPtr<Class> get_fiber_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Fiber"), snow::get_type<Fiber>());
			SN_DEFINE_METHOD(cls, "initialize", bindings::fiber_initialize);
			SN_DEFINE_METHOD(cls, "inspect", bindings::fiber_inspect);
			SN_DEFINE_METHOD(cls, "to_string", bindings::fiber_inspect);
			SN_DEFINE_METHOD(cls, "resume", bindings::fiber_resume);
			SN_DEFINE_METHOD(cls, "__call__", bindings::fiber_resume);
			SN_DEFINE_METHOD(cls, "each", bindings::fiber_each);
			SN_DEFINE_PROPERTY(cls, "running?", bindings::fiber_is_running, NULL);
			SN_DEFINE_PROPERTY(cls, "started?", bindings::fiber_is_started, NULL);
			root = gc_create_root(cls);
		}
		return *root;
	}
}