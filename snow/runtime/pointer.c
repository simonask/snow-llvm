#include "snow/pointer.h"
#include "snow/gc.h"
#include "snow/function.h"
#include "snow/str.h"

SnPointer* snow_wrap_pointer(void* ptr, SnPointerCopyFunc copy_func, SnPointerFreeFunc free_func) {
	SnPointer* obj = SN_GC_ALLOC_OBJECT(SnPointer);
	obj->ptr = ptr;
	obj->copy = copy_func;
	obj->free = free_func;
	return obj;
}

SnPointer* snow_pointer_copy(SnPointer* other) {
	SnPointer* obj = SN_GC_ALLOC_OBJECT(SnPointer);
	obj->ptr = other->copy ? other->copy(other->ptr) : other->ptr;
	obj->copy = other->copy;
	obj->free = other->free;
	return obj;
}

void snow_pointer_free(SnPointer* ptr) {
	if (ptr->ptr) {
		if (ptr->free) ptr->free(ptr->ptr);
		ptr->ptr = NULL;
	}
}

static VALUE pointer_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	SnPointer* ptr = (SnPointer*)self;
	if (snow_type_of(ptr) == SnPointerType) {
		return snow_string_format("[Pointer:%p copy:%p free:%p]", ptr->ptr, ptr->copy, ptr->free);
	}
	return snow_string_format("[Pointer@%p]", self);
}

SnObject* snow_create_pointer_prototype() {
	SnObject* proto = snow_create_object(NULL);
	
	SN_DEFINE_METHOD(proto, "inspect", pointer_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", pointer_inspect, 0);
	
	return proto;
}