#include "snow/function.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/vm.h"

typedef struct SnFunctionData {
	void* jit_func;
	SnFunctionSignature* signature;
	SnArrayRef literals;
} SnFunctionData;

#define GET_DATA(VAR, OBJ) SnFunctionData* VAR = (SnFunctionData*)(OBJ)->data
#define GET_RDATA(VAR, REF) GET_DATA(VAR, (REF).obj)

static void function_initialize(SN_P p, SnObject* obj) {
	GET_DATA(data, obj);
	data->jit_func = NULL;
}

static void function_finalize(SN_P p, SnObject* obj) {
	// XXX: What to do?
}

static void function_copy(SN_P p, SnObject* copy, const SnObject* original) {
	GET_DATA(a, copy);
	GET_DATA(b, original);
	a->jit_func = b->jit_func;
}

static void function_for_each_root(SN_P p, VALUE self, SnForEachRootCallbackFunc func, void* userdata) {
	GET_DATA(data, (SnObject*)self);
	
	// literals
	SnObject* literal_array = snow_array_as_object(data->literals);
	literal_array->type->for_each_root(p, literal_array, func, userdata);
}

static VALUE function_get_member(SN_P p, VALUE self, SnSymbol member) {
	return NULL;
}

static VALUE function_set_member(SN_P p, VALUE self, SnSymbol member, VALUE val) {
	return NULL;
}

static VALUE function_call(SN_P p, VALUE functor, VALUE self, struct SnArguments* args) {
	GET_DATA(data, (SnObject*)functor);
	return snow_vm_call_function(p, data->jit_func, snow_object_as_function((SnObject*)functor), self, args);
}

static SnType FunctionType;
const SnType* snow_get_function_type() {
	static const SnType* type = NULL;
	if (!type) {
		FunctionType.size = sizeof(SnFunctionData);
		FunctionType.initialize = function_initialize;
		FunctionType.finalize = function_finalize;
		FunctionType.copy = function_copy;
		FunctionType.for_each_root = function_for_each_root;
		FunctionType.get_member = function_get_member;
		FunctionType.set_member = function_set_member;
		FunctionType.call = function_call;
		type = &FunctionType;
	}
	return type;
}

SnFunctionRef snow_create_function(SN_P p, void* jit_func) {
	SnObject* obj = snow_gc_create_object(p, snow_get_function_type());
	GET_DATA(data, obj);
	data->jit_func = jit_func;
	return snow_object_as_function(obj);
}

SnFunctionSignature* snow_function_get_signature(SN_P p, SnFunctionRef ref) {
	GET_RDATA(data, ref);
	return data->signature;
}

SnFunctionRef snow_object_as_function(SnObject* obj) {
	ASSERT(obj->type == snow_get_function_type());
	SnFunctionRef ref;
	ref.obj = obj;
	return ref;
}

SnObject* snow_function_as_object(SnFunctionRef ref) {
	return ref.obj;
}