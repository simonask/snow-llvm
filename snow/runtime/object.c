#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"
#include "snow/map.h"
#include "snow/gc.h"
#include "snow/array.h"
#include "snow/exception.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/snow.h"

static int compare_properties(const void* _a, const void* _b) {
	const SnProperty* a = (const SnProperty*)_a;
	const SnProperty* b = (const SnProperty*)_b;
	return a->name - b->name;
}

SnObject* snow_create_object(SnObject* prototype) {
	SnObject* obj = SN_GC_ALLOC_OBJECT(SnObject);
	obj->prototype = prototype;
	obj->members = NULL;
	obj->properties = NULL;
	obj->num_properties = 0;
	obj->included_modules = NULL;
	return obj;
}

void snow_finalize_object(SnObject* obj) {
	free(obj->properties);
}

VALUE snow_object_get_member(SnObject* obj, VALUE self, SnSymbol member) {
	VALUE vmember = snow_symbol_to_value(member);
	while (obj) {
		VALUE v;
		if (obj->members) {
			if ((v = snow_map_get(obj->members, vmember)))
				return v;
		}
		
		if (obj->properties) {
			SnProperty key = { member, NULL, NULL };
			SnProperty* property = (SnProperty*)bsearch(&key, obj->properties, obj->num_properties, sizeof(SnProperty), compare_properties);
			if (property) {
				if (property->getter) {
					return snow_call(property->getter, self, 0, NULL);
				} else {
					// TODO: Exception
					fprintf(stderr, "ERROR: Property '%s' is write-only on object %p (self %p).\n", snow_sym_to_cstr(member), obj, self);
					return NULL;
				}
			}
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
	// First, look for properties. TODO: Find a way to do this faster, perhaps?
	SnObject* pobj = obj;
	SnProperty key = { member, NULL, NULL };
	while (pobj) {
		if (pobj->properties) {
			SnProperty* property = (SnProperty*)bsearch(&key, pobj->properties, pobj->num_properties, sizeof(SnProperty), compare_properties);
			if (property) {
				if (property->setter) {
					return snow_call(property->setter, self, 1, &value);
				} else {
					// TODO: Exception
					fprintf(stderr, "ERROR: Property '%s' is read-only on object %p (self %p).\n", snow_sym_to_cstr(member), pobj, self);
					return NULL;
				}
			}
		}
		pobj = pobj->prototype;
	}
	
	// TODO: Look for properties in included modules
	
	if (!obj->members)
		obj->members = snow_create_map_with_immediate_keys();
	snow_map_set(obj->members, snow_symbol_to_value(member), value);
	return value;
}


void snow_object_define_property(SnObject* obj, SnSymbol name, VALUE getter, VALUE setter) {
	SnProperty key = { name, NULL, NULL };
	SnProperty* existing_property = obj->num_properties ? (SnProperty*)bsearch(&key, obj->properties, obj->num_properties, sizeof(SnProperty), compare_properties) : 0;
	if (existing_property) {
		existing_property->getter = getter;
		existing_property->setter = setter;
	} else {
		obj->num_properties += 1;
		obj->properties = (SnProperty*)realloc(obj->properties, (obj->num_properties)*sizeof(SnProperty));
		size_t insertion_point = 0;
		while (insertion_point < obj->num_properties-1 && obj->properties[insertion_point].name < name) {
			++insertion_point;
		}
		memmove(obj->properties + insertion_point + 1, obj->properties + insertion_point, obj->num_properties - insertion_point - 1);
		existing_property = obj->properties + insertion_point;
		existing_property->name = name;
		existing_property->getter = getter;
		existing_property->setter = setter;
	}
}

bool snow_object_include_module(SnObject* object, SnObject* module) {
	if (!object->included_modules) {
		object->included_modules = snow_create_array();
	}
	if (snow_array_contains(object->included_modules, module)) return false;
	
	snow_array_push(object->included_modules, module);
	// TODO: Call module.included(object)
	return true;
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
