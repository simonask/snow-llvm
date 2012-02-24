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
#include "snow/module.hpp"
#include "inline-cache.hpp"

namespace snow {
	struct Function {
		const snow::FunctionDescriptor* descriptor;
		ObjectPtr<Environment> definition_scope;
		Value** variable_references;
		MethodCacheLine* method_cache_lines; // size = descriptor->num_method_calls;
		InstanceVariableCacheLine* instance_variable_cache_lines; // size = descriptor->num_instance_variable_accesses;
		AnyObjectPtr module;
		
		Function() : descriptor(NULL), variable_references(NULL), method_cache_lines(NULL), instance_variable_cache_lines(NULL) {}
		~Function() {
			delete[] variable_references;
			delete[] method_cache_lines;
			delete[] instance_variable_cache_lines;
		}
	};
	
	static void function_gc_each_root(void* priv, GCCallback callback) {
		auto function = static_cast<Function*>(priv);
		callback(function->definition_scope);

		// Reset inline caching with each GC.
		snow::assign_range(function->method_cache_lines, MethodCacheLine(), function->descriptor->num_method_calls);
		snow::assign_range(function->instance_variable_cache_lines, InstanceVariableCacheLine(), function->descriptor->num_instance_variable_accesses);
	}
	
	SN_REGISTER_CPP_TYPE(Function, function_gc_each_root)
	
	struct Environment {
		ObjectPtr<Function> function;
		Value self;
		Value* locals;
		size_t num_locals;
		Arguments args;
		
		Environment() :
			self(NULL),
			locals(NULL),
			num_locals(0)
		{
		}
		~Environment() {
			snow::dealloc_range(locals);
		}
	};
	
	static void environment_gc_each_root(void* priv, GCCallback callback) {
		auto env = static_cast<Environment*>(priv);
		callback(env->function);
		callback(env->self);
		for (size_t i = 0; i < env->num_locals; ++i) {
			callback(env->locals[i]);
		}
		for (const Value& arg: env->args) {
			callback(arg);
		}
	}
	
	SN_REGISTER_CPP_TYPE(Environment, environment_gc_each_root)

	ObjectPtr<Function> create_function_for_descriptor(const FunctionDescriptor* descriptor, ObjectPtr<Environment> definition_scope) {
		ObjectPtr<Function> function = create_object(get_function_class(), 0, NULL);
		function->descriptor = descriptor;
		function->definition_scope = definition_scope;
		if (descriptor->num_method_calls) {
			function->method_cache_lines = new MethodCacheLine[descriptor->num_method_calls];
		}
		if (descriptor->num_instance_variable_accesses) {
			function->instance_variable_cache_lines = new InstanceVariableCacheLine[descriptor->num_instance_variable_accesses];
		}
		if (definition_scope != NULL) {
			function->module = definition_scope->function->module;
		} else {
			function->module = get_global_module();
		}
		
		return function;
	}
	
	ObjectPtr<Function> create_function_for_module_entry(const FunctionDescriptor* descriptor, AnyObjectPtr module) {
		ASSERT(module != NULL);
		ObjectPtr<Function> function = create_object(get_function_class(), 0, NULL);
		function->descriptor = descriptor;
		if (descriptor->num_method_calls) {
			function->method_cache_lines = new MethodCacheLine[descriptor->num_method_calls];
		}
		if (descriptor->num_instance_variable_accesses) {
			function->instance_variable_cache_lines = new InstanceVariableCacheLine[descriptor->num_instance_variable_accesses];
		}
		function->module = module;
		return function;
	}
	
	namespace bindings {
		static VALUE function_call(const CallFrame* here, VALUE self, VALUE it) {
			CallFrame frame = *here;
			return function_call(self, &frame);
		}
		
		static VALUE function_inspect(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Function> f = self;
			return format_string("[Function@%@ name:%@ code:%@]", format::pointer(self), sym_to_cstr(f->descriptor->name), format::pointer(f->descriptor->ptr));
		}

		static VALUE environment_get_self(const CallFrame* here, VALUE self, VALUE it) {
			return ObjectPtr<Environment>(self)->self;
		}

		static VALUE environment_get_arguments(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Environment> cf = self;
			return create_array_from_range(cf->args.begin(), cf->args.end());
		}
	}
	
	static void environment_liberate(ObjectPtr<Environment> cf) {
		cf->locals = snow::duplicate_range(cf->locals, cf->num_locals);
		cf->args.take_ownership();
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
		descriptor->num_method_calls = 0;
		descriptor->num_instance_variable_accesses = 0;
		
		return create_function_for_descriptor(descriptor, NULL);
	}
	
	ObjectPtr<Class> get_function_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Function"), snow::get_type<Function>());
			root = gc_create_root(cls);
			SN_DEFINE_METHOD(cls, "__call__", bindings::function_call);
			SN_DEFINE_METHOD(cls, "inspect", bindings::function_inspect);
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
	
	Value* environment_get_locals(EnvironmentConstPtr obj) {
		return obj->locals;
	}
	
	ObjectPtr<Function> environment_get_function(EnvironmentConstPtr obj) {
		return obj->function;
	}

	Value environment_get_self(EnvironmentConstPtr env) {
		return env->self;
	}

	ObjectPtr<const Array> environment_get_arguments(EnvironmentConstPtr env) {
		return create_array_from_range(env->args.begin(), env->args.end());
	}
	
	Value* get_locals_from_higher_lexical_scope(const CallFrame* frame, size_t num_levels) {
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
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Environment"), get_type<Environment>());
			SN_DEFINE_PROPERTY(cls, "self", bindings::environment_get_self, NULL);
			SN_DEFINE_PROPERTY(cls, "arguments", bindings::environment_get_arguments, NULL);
			root = gc_create_root(cls);
		}
		return *root;
	}
	
	ObjectPtr<Function> value_to_function(Value val, Value* out_new_self) {
		Value functor = val;
		while (!snow::value_is_of_type(functor, get_type<Function>())) {
			ObjectPtr<Class> cls = get_class(functor);
			MethodQueryResult method;
			if (class_lookup_method(cls, snow::sym("__call__"), &method)) {
				*out_new_self = functor;
				if (method.type == MethodTypeFunction) {
					functor = method.result;
				} else if (method.type == MethodTypeProperty) {
					if (method.result == NULL) {
						throw_exception_with_description("Property __call__ is read-only on class %@.", class_get_name(cls), format::pointer(cls));
					}
					functor = call(method.result, *out_new_self, 0, NULL);
				} else {
					throw_exception_with_description("Object %@ of class %@ is not a function, and does not respond to __call__.", value_inspect(functor), class_get_name(cls), format::pointer(cls));
				}
			} else {
				throw_exception_with_description("Object %@ of class %@ is not a function, and does not respond to __call__.", value_inspect(functor), class_get_name(cls), format::pointer(cls));
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
	
	Value function_call(ObjectPtr<Function> function, CallFrame* frame) {
		const Arguments* args = frame->args;
		// Allocate locals
		const FunctionDescriptor* descriptor = function->descriptor;
		size_t num_locals = descriptor->num_locals;
		SN_STACK_ARRAY(Value, locals, num_locals);
		// initialize call frame for use with this function
		frame->function = function;
		frame->locals = locals;
		
		// Copy arguments to locals
		// TODO: Named args
		if (frame->args)
			snow::copy_range<Value>(locals, args->begin(), args->size() < num_locals ? args->size() : num_locals);

		// Call the function.
		CallFramePusher call_frame_pusher(frame);
		return descriptor->ptr(frame, frame->self, args->size() ? *args->begin() : NULL);
	}
	
	Symbol function_get_name(ObjectPtr<const Function> function) {
		return function->descriptor->name;
	}
	
	size_t function_get_num_locals(ObjectPtr<const Function> function) {
		return function->descriptor->num_locals;
	}
	
	ObjectPtr<Environment> function_get_definition_scope(ObjectPtr<const Function> function) {
		return function->definition_scope;
	}
	
	MethodCacheLine* function_get_method_cache_lines(ObjectPtr<const Function> function) {
		return function->method_cache_lines;
	}
	
	InstanceVariableCacheLine* function_get_instance_variable_cache_lines(ObjectPtr<const Function> function) {
		return function->instance_variable_cache_lines;
	}
	
	AnyObjectPtr function_get_module(ObjectPtr<const Function> function) {
		return function->module;
	}
	
	static VALUE method_proxy_call(const CallFrame* here, VALUE self, VALUE it) {
		ASSERT(is_object(self));
		auto obj = object_get_instance_variable(self, snow::sym("object"));
		auto method = object_get_instance_variable(self, snow::sym("method"));
		return call_with_arguments(method, obj, *here->args);
	}

	AnyObjectPtr create_method_proxy(Value self, Value method) {
		// TODO: Use a real class for proxy methods
		AnyObjectPtr obj = create_object(get_object_class(), 0, NULL);
		SN_OBJECT_DEFINE_METHOD(obj, "__call__", method_proxy_call);
		object_set_instance_variable(obj, snow::sym("object"), self);
		object_set_instance_variable(obj, snow::sym("method"), method);
		return obj;
	}
}
