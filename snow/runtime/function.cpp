#include "snow/function.hpp"
#include "internal.h"
#include "snow/util.hpp"
#include "snow/objectptr.hpp"
#include "function-internal.hpp"
#include "fiber-internal.hpp"
#include "snow/snow.hpp"
#include "snow/class.hpp"
#include "snow/exception.hpp"
#include "snow/array.hpp"

namespace snow {
	struct Function {
		const snow::FunctionDescriptor* descriptor;
		ObjectPtr<Environment> definition_scope;
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
	
	SN_REGISTER_CPP_TYPE(Function, function_gc_each_root)
	
	struct Environment {
		ObjectPtr<Function> function;
		VALUE self;
		VALUE* locals;
		size_t num_locals;
		SnArguments args;
		
		Environment() :
			self(NULL),
			locals(NULL),
			num_locals(0)
		{
			memset(&args, 0, sizeof(SnArguments));
		}
		~Environment() {
			snow::dealloc_range(locals);
			snow::dealloc_range(args.names);
			snow::dealloc_range(args.data);
		}
	};
	
	
	void environment_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		#define CALLBACK(ROOT) callback(reinterpret_cast<VALUE*>(&(ROOT)))
		Environment* priv = (Environment*)data;
		CALLBACK(priv->function);
		CALLBACK(priv->self);
		for (size_t i = 0; i < priv->num_locals; ++i) {
			CALLBACK(priv->locals[i]);
		}
		for (size_t i = 0; i < priv->args.size; ++i) {
			CALLBACK(priv->args.data[i]);
		}
	}
	
	SN_REGISTER_CPP_TYPE(Environment, environment_gc_each_root)

	ObjectPtr<Function> create_function_for_descriptor(const FunctionDescriptor* descriptor, const ObjectPtr<Environment>& definition_scope) {
		ObjectPtr<Function> function = create_object(get_function_class(), 0, NULL);
		function->descriptor = descriptor;
		function->definition_scope = definition_scope;
		if (descriptor->num_variable_references) {
			function->variable_references = new VALUE*[descriptor->num_variable_references];
			// TODO: Set up references
			memset(function->variable_references, NULL, sizeof(VALUE) * descriptor->num_variable_references);
		}
		return function;
	}
	
	namespace bindings {
		static VALUE function_call(const CallFrame* here, VALUE self, VALUE it) {
			SN_CHECK_CLASS(self, Function, __call__);
			CallFrame frame = *here;
			return function_call(self, &frame);
		}

		static VALUE environment_get_self(const CallFrame* here, VALUE self, VALUE it) {
			return ObjectPtr<Environment>(self)->self;
		}

		static VALUE environment_get_arguments(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Environment> cf = self;
			return create_array_from_range(cf->args.data, cf->args.data + cf->args.size);
		}
	}
	
	static void environment_liberate(const ObjectPtr<Environment>& cf) {
		cf->locals = snow::duplicate_range(cf->locals, cf->num_locals);
		cf->args.names = snow::duplicate_range(cf->args.names, cf->args.num_names);
		cf->args.data = snow::duplicate_range(cf->args.data, cf->args.size);
	}

	ObjectPtr<Function> create_function(FunctionPtr ptr, Symbol name) {
		snow::FunctionDescriptor* descriptor = new snow::FunctionDescriptor;
		descriptor->ptr = ptr;
		descriptor->name = name;
		descriptor->return_type = AnyType;
		descriptor->num_params = 0; // TODO: Named parameter support
		descriptor->param_types = NULL;
		descriptor->param_names = NULL;
		descriptor->num_locals = 0;
		descriptor->num_variable_references = 0;
		descriptor->variable_references = NULL;
		
		return create_function_for_descriptor(descriptor, NULL);
	}
	
	ObjectPtr<Class> get_function_class() {
		static SnObject** root = NULL;
		if (!root) {
			SnObject* cls = create_class_for_type(snow::sym("Function"), snow::get_type<Function>());
			root = gc_create_root(cls);
			SN_DEFINE_METHOD(cls, "__call__", bindings::function_call);
		}
		return *root;
	}
	
	ObjectPtr<Environment> call_frame_environment(CallFrame* frame) {
		if (frame->environment != NULL) return frame->environment;
		
		ObjectPtr<Environment> obj = create_object(get_environment_class(), 0, NULL);
		obj->function = frame->function;
		obj->self = frame->self;
		obj->locals = frame->locals; // shared until call_frame_liberate
		obj->num_locals = function_get_num_locals(obj->function);
		obj->args = *frame->args; // shared until call_frame_liberate
		frame->environment = obj;
		return obj;
	}
	
	VALUE* environment_get_locals(EnvironmentConstPtr obj) {
		return obj->locals;
	}
	
	ObjectPtr<Function> environment_get_function(EnvironmentConstPtr obj) {
		return obj->function;
	}
	
	VALUE* get_locals_from_higher_lexical_scope(const CallFrame* frame, size_t num_levels) {
		if (num_levels == 0) return frame->locals;
		ObjectPtr<Function> function = frame->function;
	 	ObjectPtr<Environment> definition_scope;
		for (size_t i = 0; i < num_levels; ++i) {
			definition_scope = function_get_definition_scope(function);
			function = environment_get_function(definition_scope);
		}
		return environment_get_locals(definition_scope);
	}
	
	ObjectPtr<Class> get_environment_class() {
		static SnObject** root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Environment"), get_type<Environment>());
			SN_DEFINE_PROPERTY(cls, "self", bindings::environment_get_self, NULL);
			SN_DEFINE_PROPERTY(cls, "arguments", bindings::environment_get_arguments, NULL);
			root = gc_create_root(cls);
		}
		return *root;
	}
	
	ObjectPtr<Function> value_to_function(VALUE val, VALUE* out_new_self) {
		VALUE functor = val;
		while (!snow::value_is_of_type(functor, get_type<Function>())) {
			ObjectPtr<Class> cls = get_class(functor);
			Method method;
			if (class_lookup_method(cls, snow::sym("__call__"), &method)) {
				*out_new_self = functor;
				if (method.type == MethodTypeFunction) {
					functor = method.function;
				} else {
					if (method.property->getter == NULL) {
						throw_exception_with_description("Property __call__ is read-only on class %s@p.", class_get_name(cls), cls.value());
					}
					functor = call(method.property->getter, *out_new_self, 0, NULL);
				}
			} else {
				throw_exception_with_description("Object %p of class %s@%p is not a function, and does not respond to __call__.", functor, class_get_name(cls), cls.value());
			}
		}
		
		if (is_truthy(functor)) {
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
			CallFramePusher(CallFrame* frame) : frame(frame) {
				push_call_frame(frame);
			}
			~CallFramePusher() {
				if (frame->environment != NULL)
					environment_liberate(frame->environment);
				pop_call_frame(frame);
			}

			CallFrame* frame;
		};
	}
	
	VALUE function_call(const ObjectPtr<Function>& function, CallFrame* frame) {
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
			Symbol name = function->descriptor->param_names[i];
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
	
	Symbol function_get_name(const ObjectPtr<const Function>& function) {
		return function->descriptor->name;
	}
	
	size_t function_get_num_locals(const ObjectPtr<const Function>& function) {
		return function->descriptor->num_locals;
	}
	
	ObjectPtr<Environment> function_get_definition_scope(const ObjectPtr<const Function>& function) {
		return function->definition_scope;
	}
	
	static VALUE method_proxy_call(const CallFrame* here, VALUE self, VALUE it) {
		ASSERT(is_object(self));
		SnObject* s = (SnObject*)self;
		VALUE obj = object_get_instance_variable(s, snow::sym("object"));
		VALUE method = object_get_instance_variable(s, snow::sym("method"));
		return call_with_arguments(method, obj, here->args);
	}

	SnObject* create_method_proxy(VALUE self, VALUE method) {
		// TODO: Use a real class for proxy methods
		SnObject* obj = create_object(get_object_class(), 0, NULL);
		object_define_method(obj, "__call__", method_proxy_call);
		object_set_instance_variable(obj, snow::sym("object"), self);
		object_set_instance_variable(obj, snow::sym("method"), method);
		return obj;
	}
}