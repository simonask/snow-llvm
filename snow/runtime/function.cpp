#include "snow/function.hpp"
#include "internal.h"
#include "util.hpp"
#include "objectptr.hpp"
#include "function-internal.hpp"
#include "fiber-internal.h"
#include "snow/snow.h"
#include "snow/class.hpp"
#include "snow/exception.h"
#include "snow/array.hpp"

namespace snow {
	struct Function {
		const snow::FunctionDescriptor* descriptor;
		ObjectPtr<CallFrame> definition_scope;
		VALUE** variable_references; // size: descriptor->num_variable_references. TODO: Consider garbage collection?
		
		Function() : descriptor(NULL), variable_references(NULL) {}
	};
	
	void function_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		Function* priv = (Function*)data;
		callback((VALUE*)&priv->definition_scope);
		for (size_t i = 0; i < priv->descriptor->num_variable_references; ++i) {
			callback(priv->variable_references[i]);
		}
	}
	
	struct CallFrame {
		ObjectPtr<Function> function;
		VALUE self;
		VALUE* locals;
		size_t num_locals;
		SnArguments args;
		
		CallFrame() :
			self(NULL),
			locals(NULL),
			num_locals(0)
		{
			memset(&args, 0, sizeof(SnArguments));
		}
		~CallFrame() {
			snow::dealloc_range(locals);
			snow::dealloc_range(args.names);
			snow::dealloc_range(args.data);
		}
	};
	
	
	void call_frame_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		#define CALLBACK(ROOT) callback(reinterpret_cast<VALUE*>(&(ROOT)))
		CallFrame* priv = (CallFrame*)data;
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

SN_REGISTER_CPP_TYPE(Function, function_gc_each_root)
SN_REGISTER_CPP_TYPE(CallFrame, call_frame_gc_each_root)

namespace snow {
	SnObject* create_function_for_descriptor(const FunctionDescriptor* descriptor, SnObject* definition_scope) {
		ObjectPtr<Function> function = snow_create_object(snow_get_function_class(), 0, NULL);
		function->descriptor = descriptor;
		function->definition_scope = definition_scope;
		if (descriptor->num_variable_references) {
			function->variable_references = new VALUE*[descriptor->num_variable_references];
			// TODO: Set up references
			memset(function->variable_references, NULL, sizeof(VALUE) * descriptor->num_variable_references);
		}
		return function;
	}
	
	static VALUE function_call(const SnCallFrame* here, VALUE self, VALUE it) {
		SN_CHECK_CLASS(self, Function, __call__);
		SnCallFrame frame = *here;
		return snow_function_call((SnObject*)self, &frame);
	}

	static VALUE call_frame_get_self(const SnCallFrame* here, VALUE self, VALUE it) {
		return ObjectPtr<CallFrame>(self)->self;
	}

	static VALUE call_frame_get_arguments(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<CallFrame> cf = self;
		return snow_create_array_from_range(cf->args.data, cf->args.data + cf->args.size);
	}

	static void call_frame_liberate(SnObject* call_frame) {
		ObjectPtr<CallFrame> cf = call_frame;
		cf->locals = snow::duplicate_range(cf->locals, cf->num_locals);
		cf->args.names = snow::duplicate_range(cf->args.names, cf->args.num_names);
		cf->args.data = snow::duplicate_range(cf->args.data, cf->args.size);
	}
}

CAPI {
	using namespace snow;
	
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
			SnObject* cls = snow_create_class_for_type(snow_sym("Function"), snow::get_type<Function>());
			root = snow_gc_create_root(cls);
			snow_class_define_method(cls, "__call__", function_call);
		}
		return *root;
	}
	
	SnObject* snow_call_frame_as_object(SnCallFrame* frame) {
		if (frame->as_object) return frame->as_object;
		
		ObjectPtr<CallFrame> obj = snow_create_object(snow_get_call_frame_class(), 0, NULL);
		obj->function = frame->function;
		obj->self = frame->self;
		obj->locals = frame->locals; // shared until call_frame_liberate
		obj->num_locals = snow_function_get_num_locals(obj->function);
		obj->args = *frame->args; // shared until call_frame_liberate
		frame->as_object = obj;
		return obj;
	}
	
	VALUE* snow_call_frame_get_locals(const SnObject* obj) {
		return ObjectPtr<const CallFrame>(obj)->locals;
	}
	
	SnObject* snow_call_frame_get_function(const SnObject* obj) {
		return ObjectPtr<const CallFrame>(obj)->function;
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
			SnObject* cls = snow_create_class_for_type(snow_sym("CallFrame"), get_type<CallFrame>());
			snow_class_define_property(cls, "self", call_frame_get_self, NULL);
			snow_class_define_property(cls, "arguments", call_frame_get_arguments, NULL);
			root = snow_gc_create_root(cls);
		}
		return *root;
	}
	
	SnObject* snow_value_to_function(VALUE val, VALUE* out_new_self) {
		VALUE functor = val;
		while (!snow::value_is_of_type(functor, get_type<Function>())) {
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
		
		if (snow_eval_truth(functor)) {
			ObjectPtr<Function> function = functor;
			if (*out_new_self == NULL && function->definition_scope != NULL) {
				// If self isn't given, pick the self from when the function was defined.
				*out_new_self = function->definition_scope->self;
			}
			return function;
		}
		
		return NULL; // Value cannot be converted to function.
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
	
	VALUE snow_function_call(SnObject* func, SnCallFrame* frame) {
		ObjectPtr<Function> function = func;
		ASSERT(function != NULL); // function is not a Function
		
		// Allocate locals
		size_t num_locals = function->descriptor->num_locals;
		VALUE locals[num_locals];
		// initialize call frame for use with this function
		frame->function = function;
		frame->locals = locals;
		
		// TODO: There *must* be a way to optimize this!
		// First, assign named arguments to the proper locals
		snow::assign_range<VALUE>(locals, NULL, num_locals);
		size_t num_params = function->descriptor->num_params;
		ASSERT(num_params <= num_locals);
		const SnArguments* args = frame->args;
		bool taken_for_named_args[args->size];
		snow::assign_range<bool>(taken_for_named_args, false, args->size);
		for (size_t i = 0; i < num_params; ++i) {
			SnSymbol name = function->descriptor->param_names[i];
			for (size_t j = 0; j < args->num_names; ++j) {
				if (name == args->names[j]) {
					locals[i] = args->data[j];
					taken_for_named_args[j] = true;
					break;
				}
			}
		}
		// Second, assign remaining arguments (named or not) to the remaining locals
		if (args->size) {
			size_t arg_i = 0;
			for (size_t i = 0; i < num_params; ++i) {
				if (locals[i] != NULL) continue; // already set by named arg
				while (taken_for_named_args[arg_i]) ++arg_i;
				if (arg_i >= args->size) break;
				locals[i] = args->data[arg_i];
			}
		}

		// Call the function.
		CallFramePusher call_frame_pusher(frame);
		return function->descriptor->ptr(frame, frame->self, args->size ? args->data[0] : NULL);
	}
	
	SnSymbol snow_function_get_name(const SnObject* function) {
		return ObjectPtr<const Function>(function)->descriptor->name;
	}
	
	size_t snow_function_get_num_locals(const SnObject* function) {
		return ObjectPtr<const Function>(function)->descriptor->num_locals;
	}
	
	SnObject* snow_function_get_definition_scope(const SnObject* function) {
		return ObjectPtr<const Function>(function)->definition_scope;
	}
	
	static VALUE method_proxy_call(const SnCallFrame* here, VALUE self, VALUE it) {
		ASSERT(snow_is_object(self));
		SnObject* s = (SnObject*)self;
		VALUE obj = snow_object_get_instance_variable(s, snow_sym("object"));
		VALUE method = snow_object_get_instance_variable(s, snow_sym("method"));
		return snow_call_with_arguments(method, obj, here->args);
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