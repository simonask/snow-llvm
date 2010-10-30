#include "snow/object.h"
#include "snow/type.h"
#include "snow/array.h"
#include "snow/gc.h"
#include "snow/vm.h"
#include "snow/function.h"

SnFunction* snow_create_function(SnFunctionDescriptor* descriptor) {
	SnFunction* obj = (SnFunction*)snow_gc_alloc_object(SnFunctionType);
	obj->descriptor = descriptor;
	return obj;
}

void snow_finalize_function(SnFunction* func) {
}

VALUE snow_function_call(SnFunction* func, VALUE self, struct SnArguments* args) {
	// TODO
	return SN_NIL;
}