#include "snow/function.h"
#include "internal.h"
#include "util.hpp"
#include "function-internal.hpp"
#include "fiber-internal.h"
#include "snow/snow.h"
#include "snow/class.h"
#include "snow/exception.h"
#include "snow/array.h"

namespace {
	struct FunctionPrivate {
		const snow::FunctionDescriptor* descriptor;
		SnObject* definition_scope;
		VALUE** variable_references; // size: descriptor->num_variable_references. TODO: Consider garbage collection?
		
		FunctionPrivate() : descriptor(NULL), definition_scope(NULL), variable_references(NULL) {}
	};
	
	void function_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		FunctionPrivate* priv = (FunctionPrivate*)data;
		callback((VALUE*)&priv->definition_scope);
		for (size_t i = 0; i < priv->descriptor->num_variable_references; ++i) {
			callback(priv->variable_references[i]);
		}
	}
	
	struct CallFramePrivate {
		SnObject* function;
		VALUE self;
		VALUE* locals;
		size_t num_locals;
		SnArguments args;
		
		CallFramePrivate() :
			function(NULL),
			self(NULL),
			locals(NULL),
			num_locals(0)
		{
			memset(&args, 0, sizeof(SnArguments));
		}
		~CallFramePrivate() {
			snow::dealloc_range(locals);
			snow::dealloc_range(args.names);
			snow::dealloc_range(args.data);
		}
	};
	
	void call_frame_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		#define CALLBACK(ROOT) callback(reinterpret_cast<VALUE*>(&(ROOT)))
		CallFramePrivate* priv = (CallFramePrivate*)data;
		CALLBACK(priv->function);
		CALLBACK(priv->self);
		for (size_t i = 0; i < priv->num_locals; ++i) {
			CALLBACK(priv->locals[i]);
		}
		for (size_t i = 0; i < priv->args.size; ++i) {
			CALLBACK(priv->args.data[i]);
		}
	}
}

CAPI SnObjectType SnFunctionType = {
	.data_size = sizeof(FunctionPrivate),
	.initialize = snow::construct<FunctionPrivate>,
	.finalize = snow::destruct<FunctionPrivate>,
	.copy = snow::assign<FunctionPrivate>,
	.gc_each_root = function_gc_each_root,
};

CAPI SnObjectType SnCallFrameType = {
	.data_size = sizeof(CallFramePrivate),
	.initialize = snow::construct<CallFramePrivate>,
	.finalize = snow::destruct<CallFramePrivate>,
	.copy = snow::assign<CallFramePrivate>,
	.gc_each_root = call_frame_gc_each_root,
};

namespace snow {
	SnObject* create_function_for_descriptor(const FunctionDescriptor* descriptor, SnObject* definition_scope) {
		SnObject* function = snow_create_object(snow_get_function_class(), 0, NULL);
		FunctionPrivate* priv = snow::object_get_private<FunctionPrivate>(function, SnFunctionType);
		ASSERT(priv);
		priv->descriptor = descriptor;
		priv->definition_scope = definition_scope;
		if (descriptor->num_variable_references) {
			priv->variable_references = new VALUE*[descriptor->num_variable_references];
			// TODO: Set up references
			memset(priv->variable_references, NULL, sizeof(VALUE) * descriptor->num_variable_references);
		}
		return function;
	}
}

static VALUE function_call(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, Function, __call__);
	SnCallFrame frame = *here;
	return snow_function_call((SnObject*)self, &frame);
}

static VALUE call_frame_get_self(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, CallFrame, self);
	CallFramePrivate* priv = snow::value_get_private<CallFramePrivate>(self, SnCallFrameType);
	ASSERT(priv);
	return priv->self;
}

static VALUE call_frame_get_arguments(const SnCallFrame* here, VALUE self, VALUE it) {
	SN_CHECK_CLASS(self, CallFrame, self);
	CallFramePrivate* priv = snow::value_get_private<CallFramePrivate>(self, SnCallFrameType);
	ASSERT(priv);
	// TODO: Real Arguments class
	return snow_create_array_from_range(priv->args.data, priv->args.data + priv->args.size);
}

static void call_frame_liberate(SnObject* call_frame) {
	CallFramePrivate* priv = snow::object_get_private<CallFramePrivate>(call_frame, SnCallFrameType);
	ASSERT(priv);
	priv->locals = snow::duplicate_range(priv->locals, priv->num_locals);
	priv->args.names = snow::duplicate_range(priv->args.names, priv->args.num_names);
	priv->args.data = snow::duplicate_range(priv->args.data, priv->args.size);
}

CAPI {
	SnObject* snow_create_function(SnFunctionPtr ptr, SnSymbol name) {
		snow::FunctionDescriptor* descriptor = new snow::FunctionDescriptor;
		descriptor->ptr = ptr;
		descriptor->name = name;
		descriptor->return_type = SnAnyType;
		descriptor->num_params = 0; // TODO: Named parameter support
		descriptor->param_types = NULL;
		descriptor->param_names = NULL;
		descriptor->num_locals = 0;
		descriptor->num_variable_references = 0;
		descriptor->variable_references = NULL;
		
		return create_function_for_descriptor(descriptor, NULL);
	}
	
	SnObject* snow_get_function_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = snow_create_class_for_type(snow_sym("Function"), &SnFunctionType);
			root = snow_gc_create_root(cls);
			snow_class_define_method(cls, "__call__", function_call);
		}
		return *root;
	}
	
	SnObject* snow_call_frame_as_object(const SnCallFrame* frame) {
		if (frame->as_object) return frame->as_object;
		
		SnObject* obj = snow_create_object(snow_get_call_frame_class(), 0, NULL);
		CallFramePrivate* priv = snow::object_get_private<CallFramePrivate>(obj, SnCallFrameType);
		priv->function = frame->function;
		priv->self = frame->self;
		priv->locals = frame->locals; // shared until call_frame_liberate
		priv->num_locals = snow_function_get_num_locals(priv->function);
		priv->args = frame->args; // shared until call_frame_liberate
		frame->as_object = obj;
		return obj;
	}
	
	VALUE* snow_call_frame_get_locals(const SnObject* obj) {
		const CallFramePrivate* priv = snow::object_get_private<CallFramePrivate>(obj, SnCallFrameType);
		ASSERT(priv);
		return priv->locals;
	}
	
	SnObject* snow_call_frame_get_function(const SnObject* obj) {
		const CallFramePrivate* priv = snow::object_get_private<CallFramePrivate>(obj, SnCallFrameType);
		ASSERT(priv);
		return priv->function;
	}
	
	VALUE* snow_get_locals_from_higher_lexical_scope(const SnCallFrame* frame, size_t num_levels) {
		if (num_levels == 0) return frame->locals;
		SnObject* function = frame->function;
		SnObject* definition_scope = NULL;
		for (size_t i = 0; i < num_levels; ++i) {
			definition_scope = snow_function_get_definition_scope(function);
			function = snow_call_frame_get_function(definition_scope);
		}
		return snow_call_frame_get_locals(definition_scope);
	}
	
	SnObject* snow_get_call_frame_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = snow_create_class_for_type(snow_sym("CallFrame"), &SnCallFrameType);
			snow_class_define_property(cls, "self", call_frame_get_self, NULL);
			snow_class_define_property(cls, "arguments", call_frame_get_arguments, NULL);
			root = snow_gc_create_root(cls);
		}
		return *root;
	}
	
	SnObject* snow_value_to_function(VALUE val, VALUE* out_new_self) {
		VALUE functor = val;
		while (!snow::value_is_of_type(functor, SnFunctionType)) {
			SnObject* cls = snow_get_class(functor);
			SnMethod method;
			if (snow_class_lookup_method(cls, snow_sym("__call__"), &method)) {
				*out_new_self = functor;
				if (method.type == SnMethodTypeFunction) {
					functor = method.function;
				} else {
					if (method.property->getter == NULL) {
						snow_throw_exception_with_description("Property __call__ is read-only on class %s@p.", snow_class_get_name(cls), cls);
					}
					functor = snow_call(method.property->getter, *out_new_self, 0, NULL);
				}
			} else {
				snow_throw_exception_with_description("Object %p of class %s@%p is not a function, and does not respond to __call__.", functor, snow_class_get_name(cls), cls);
			}
		}
		return snow_eval_truth(functor) ? (SnObject*)functor : NULL;
	}
	
	namespace {
		struct CallFramePusher {
			CallFramePusher(SnCallFrame* frame) : frame(frame) {
				snow_push_call_frame(frame);
			}
			~CallFramePusher() {
				if (frame->as_object)
					call_frame_liberate(frame->as_object);
				snow_pop_call_frame(frame);
			}

			SnCallFrame* frame;
		};
	}
	
	VALUE snow_function_call(SnObject* function, SnCallFrame* frame) {
		FunctionPrivate* priv = snow::object_get_private<FunctionPrivate>(function, SnFunctionType);
		ASSERT(priv); // function is not a Function
		
		CallFramePusher call_frame_pusher(frame);
		return priv->descriptor->ptr(frame, frame->self, frame->args.size ? frame->args.data[0] : NULL);
	}
	
	SnSymbol snow_function_get_name(const SnObject* function) {
		const FunctionPrivate* priv = snow::object_get_private<FunctionPrivate>(function, SnFunctionType);
		ASSERT(priv); // function is not a Function
		return priv->descriptor->name;
	}
	
	size_t snow_function_get_num_locals(const SnObject* function) {
		const FunctionPrivate* priv = snow::object_get_private<FunctionPrivate>(function, SnFunctionType);
		ASSERT(priv); // function is not a Function
		return priv->descriptor->num_locals;
	}
	
	SnObject* snow_function_get_definition_scope(const SnObject* function) {
		const FunctionPrivate* priv = snow::object_get_private<FunctionPrivate>(function, SnFunctionType);
		ASSERT(priv); // function is not a Function
		return priv->definition_scope;
	}
	
	static VALUE method_proxy_call(const SnCallFrame* here, VALUE self, VALUE it) {
		ASSERT(snow_is_object(self));
		SnObject* s = (SnObject*)self;
		VALUE obj = snow_object_get_instance_variable(s, snow_sym("object"));
		VALUE method = snow_object_get_instance_variable(s, snow_sym("method"));
		return snow_call_with_arguments(method, obj, &here->args);
	}

	SnObject* snow_create_method_proxy(VALUE self, VALUE method) {
		// TODO: Use a real class for proxy methods
		SnObject* obj = snow_create_object(snow_get_object_class(), 0, NULL);
		snow_object_define_method(obj, "__call__", method_proxy_call);
		snow_object_set_instance_variable(obj, snow_sym("object"), self);
		snow_object_set_instance_variable(obj, snow_sym("method"), method);
		return obj;
	}
	
}