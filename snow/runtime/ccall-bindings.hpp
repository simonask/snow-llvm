#pragma once
#ifndef CCALL_BINDINGS_HPP_I36HJ0KQ
#define CCALL_BINDINGS_HPP_I36HJ0KQ

#include "snow/snow.hpp"
#include "snow/class.hpp"
#include "snow/array.hpp"
#include "snow/object.hpp"
#include "snow/function.hpp"
#include "inline-cache.hpp"
#include "function-internal.hpp"
#include "internal.h"

namespace snow {
	namespace ccall {
		void class_lookup_method(Object* cls, Symbol name, MethodQueryResult* out_method) {
			snow::class_lookup_method(cls, name, out_method);
		}
		
		VALUE call_method(VALUE method, VALUE self, size_t num_args, const VALUE* args, MethodType type, Symbol name) {
			switch (type) {
				case MethodTypeNone: {
					ASSERT(false);
					return NULL;
				}
				case MethodTypeFunction: {
					return snow::call(method, self, num_args, reinterpret_cast<const Value*>(args));
				}
				case MethodTypeProperty: {
					Value functor = snow::call(method, self, 0, NULL);
					return snow::call(functor, self, num_args, reinterpret_cast<const Value*>(args));
				}
				case MethodTypeMissing: {
					SN_STACK_ARRAY(Value, new_args, num_args+1);
					new_args[0] = symbol_to_value(name);
					snow::copy_range(new_args+1, args, num_args);
					return snow::call(method, self, num_args+1, new_args);
				}
			}
		}
		
		int32_t class_get_index_of_instance_variable(VALUE cls, Symbol name) {
			return snow::class_get_index_of_instance_variable(cls, name);
		}
		
		VALUE call_frame_environment(CallFrame* call_frame) {
			return snow::call_frame_environment(call_frame);
		}
		
		void array_push(Object* array, VALUE value) {
			snow::array_push(array, value);
		}
		
		VALUE create_function_for_descriptor(const FunctionDescriptor* descriptor, VALUE definition_environment) {
			return snow::create_function_for_descriptor(descriptor, definition_environment);
		}
		
		Object* get_class(VALUE val) {
			return snow::get_class(val);
		}
		
		VALUE call(VALUE functor, VALUE self, size_t num_args, const VALUE* args) {
			const Value* vargs = reinterpret_cast<const Value*>(args); // TODO: Consider this
			return snow::call(functor, self, num_args, vargs);
		}
		
		VALUE call_with_named_arguments(VALUE functor, VALUE self, size_t num_names, Symbol* names, size_t num_args, VALUE* args) {
			const Value* vargs = reinterpret_cast<const Value*>(args); // TODO: Consider this
			return snow::call_with_named_arguments(functor, self, num_names, names, num_args, vargs);
		}
		
		VALUE set_global(CallFrame* frame, Symbol name, VALUE val) {
			AnyObjectPtr module = function_get_module(frame->function);
			object_set_instance_variable(module, name, val);
			return val;
		}
		
		VALUE get_global(CallFrame* frame, Symbol name) {
			AnyObjectPtr module = function_get_module(frame->function);
			return object_get_instance_variable(module, name);
		}
		
		VALUE local_missing(CallFrame* here, Symbol name) {
			VALUE gl = get_global(here, name);
			if (gl != NULL) return gl;
			return snow::local_missing(here, name);
		}
		
		Object* create_method_proxy(VALUE object, VALUE method) {
			return snow::create_method_proxy(object, method);
		}
		
		Object* create_array_with_size(uint32_t size) {
			return snow::create_array_with_size(size);
		}
		
		Value* call_frame_get_locals(const CallFrame* here) {
			return here->locals;
		}
		
		int32_t object_get_or_create_index_of_instance_variable(VALUE obj, Symbol name) {
			return snow::object_get_or_create_index_of_instance_variable(obj, name);
		}
		
		int32_t object_get_index_of_instance_variable(VALUE obj, Symbol name) {
			return snow::object_get_index_of_instance_variable(obj, name);
		}
		
		VALUE object_get_instance_variable_by_index(VALUE obj, int32_t idx) {
			return snow::object_get_instance_variable_by_index(obj, idx);
		}
		
		VALUE object_set_instance_variable_by_index(VALUE obj, int32_t idx, VALUE val) {
			return snow::object_set_instance_variable_by_index(obj, idx, val);
		}
		
		VALUE object_set_property_or_define_method(VALUE obj, Symbol name, VALUE val) {
			return snow::object_set_property_or_define_method(obj, name, val);
		}
		
		VALUE call_frame_get_it(const CallFrame* frame) {
			if (frame->args != NULL) {
				return frame->args->size ? frame->args->data[0] : NULL;
			}
			return NULL;
		}
		
		MethodCacheLine* call_frame_get_method_cache_lines(const CallFrame* frame) {
			ASSERT(frame);
			return function_get_method_cache_lines(frame->function);
		}
		
		InstanceVariableCacheLine* call_frame_get_instance_variable_cache_lines(const CallFrame* frame) {
			ASSERT(frame);
			return function_get_instance_variable_cache_lines(frame->function);
		}
	}
}

#endif /* end of include guard: CCALL_BINDINGS_HPP_I36HJ0KQ */
