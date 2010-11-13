#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"
#include "snow/map.h"
#include "snow/gc.h"
#include "snow/array.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/str.h"

struct SnArguments;

SnObject* snow_create_object(SnObject* prototype) {
	SnObject* obj = SN_GC_ALLOC_OBJECT(SnObject);
	obj->prototype = prototype;
	obj->members = NULL;
	obj->properties = NULL;
	obj->included_modules = NULL;
	return obj;
}

VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol member) {
	VALUE vmember = snow_symbol_to_value(member);
	while (obj) {
		VALUE v;
		if (obj->members) {
			if ((v = snow_map_get(obj->members, vmember)))
				return v;
		}
		
		if (obj->included_modules) {
			for (size_t i = 0; i < snow_array_size(obj->included_modules); ++i) {
				SnObject* included = (SnObject*)snow_array_get(obj->included_modules, i);
				if ((v = snow_object_get_member(included, self, member)))
					return v;
			}
		}
		
		obj = obj->prototype;
	}
	return NULL;
}

VALUE snow_object_set_member(SnObject* obj, VALUE self, SnSymbol member, VALUE value) {
	if (obj != self) {
		snow_throw_exception_with_description("Trying to set member of faux object (properties not supported yet).");
		return NULL;
	}
	
	if (!obj->members)
		obj->members = snow_create_map_with_immediate_keys();
	snow_map_set(obj->members, snow_symbol_to_value(member), value);
	return value;
}


static VALUE object_inspect(SnFunctionCallContext* here, VALUE self, VALUE it) {
	char buffer[100];
	snprintf(buffer, 100, "[Object@%p]", self);
	return snow_create_string(buffer);
}

SnObject* snow_create_object_prototype() {
	SnObject* proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(proto, "inspect", object_inspect, 0);
	SN_DEFINE_METHOD(proto, "to_string", object_inspect, 0);
	return proto;
}
