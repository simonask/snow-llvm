#include "snow/class.h"
#include "snow/function.h"
#include "snow/str.h"
#include "internal.h"
#include "linkheap.hpp"
#include "util.hpp"

namespace {
	static snow::LinkHeap<SnMethod> method_allocator;
	
	SnClass* create_derived_class(SnSymbol name, SnClass* super) {
		SnClass* cls = SN_GC_ALLOC_OBJECT(SnClass);
		_snow_object_init(&cls->base, snow_get_class_class());
		cls->name = name;
		cls->internal_type = SnObjectType;
		cls->super = super;
		cls->fields = NULL;
		cls->methods = NULL;
		cls->extensions = NULL;
		cls->num_fields = 0;
		cls->num_methods = 0;
		cls->num_extensions = 0;
		if (super) {
			cls->internal_type = super->internal_type;
			
			cls->fields = snow::alloc_range<SnField>(super->num_fields);
			snow::copy_construct_range(cls->fields, super->fields, super->num_fields);
			cls->num_fields = super->num_fields;
			
			// Create method references for all methods of super.
			// This is to allow efficient lookup and inline caching, at the
			// expense of extra memory usage.
			cls->methods = snow::alloc_range<SnMethod*>(super->num_methods);
			for (size_t i = 0; i < super->num_methods; ++i) {
				// super->methods are already correctly sorted
				cls->methods[i] = method_allocator.alloc();
				cls->methods[i]->name = super->methods[i]->name;
				cls->methods[i]->type = SnMethodTypeReference;
				cls->methods[i]->reference = super->methods[i];
			}
			cls->num_methods = super->num_methods;
			
			// TODO: Consider extensions. Should they be optimized, or should
			// they always mean a full method lookup?
		}
		
		return cls;
	}
	
	ssize_t compare_method_name(const SnMethod* a, const SnMethod* b) {
		return a->name - b->name;
	}
	
	void define_method(SnClass* cls, const SnMethod* method) {
		SnMethod** end = cls->methods + cls->num_methods;
		SnMethod** pmethod = snow::binary_search_lower_bound(cls->methods, end, method, compare_method_name);
		if ((*pmethod)->name != method->name) {
			// add the method
			size_t position = pmethod - cls->methods;
			SnMethod* new_method = method_allocator.alloc();
			*new_method = *method;
			cls->methods = snow::insert_in_range(cls->methods, end, pmethod, new_method);
			++cls->num_methods;
		} else {
			// change the method
			**pmethod = *method;
		}
	}
}

CAPI {
	SnClass* snow_define_class(SnSymbol name, SnClass* super, size_t num_fields, const SnField* fields, size_t num_methods, const SnMethod* methods) {
		SnClass* cls = create_derived_class(name, super);
		for (size_t i = 0; i < num_fields; ++i) {
			snow_class_define_field(cls, fields[i].name, fields[i].flags);
		}
		for (size_t i = 0; i < num_methods; ++i) {
			define_method(cls, methods + i);
		}
		return cls;
	}
	
	void snow_finalize_class(SnClass* cls) {
		free(cls->fields);
		free(cls->methods);
		free(cls->extensions);
	}
	
	size_t snow_class_define_field(SnClass* cls, SnSymbol name, uint8_t flags) {
		ssize_t i = snow_class_index_of_field(cls, name, flags);
		if (i >= 0) return i;
		
		cls->fields = (SnField*)realloc(cls->fields, (cls->num_fields+1) * sizeof(SnField));
		const size_t idx = cls->num_fields++;
		cls->fields[idx].name = name;
		cls->fields[idx].flags = flags;
		return idx;
	}
	
	ssize_t snow_class_index_of_field(const SnClass* cls, SnSymbol name, uint8_t flags) {
		for (ssize_t i = 0; i < cls->num_fields; ++i) {
			if (cls->fields[i].name == name && cls->fields[i].flags == flags)
				return i;
		}
		return -1;
	}
	
	void snow_class_define_method(SnClass* cls, SnSymbol name, VALUE function) {
		SnMethod method = { name, SnMethodTypeFunction, .function = function };
		define_method(cls, &method);
	}
	
	void snow_class_define_property(SnClass* cls, SnSymbol name, VALUE getter, VALUE setter) {
		SnMethod method = { name, SnMethodTypeProperty, .property = { getter, setter } };
		define_method(cls, &method);
	}
	
	const SnMethod* snow_find_method(SnClass* cls, SnSymbol name) {
		SnClass* c = (SnClass*)cls;
		SnClass* object_class = snow_get_object_class();
		while (c) {
			SnMethod key = { name, SnMethodTypeFunction /* irrelevant */, .function = NULL /* irrelevant */ };
			SnMethod** end = cls->methods + cls->num_methods;
			SnMethod** method = snow::binary_search(cls->methods, end, &key, compare_method_name);
			if (method != end) {
				// TODO: Consider creating method references.
				return *method;
			}
			
			// TODO: Class extensions
			
			if (c != object_class) {
				c = c->super ? c->super : object_class;
			} else {
				c = NULL;
			}
		}
		return NULL;
	}
	
	const SnMethod* snow_resolve_method_reference(const SnMethod* method) {
		if (UNLIKELY(!method)) return NULL;
		const SnMethod* m = method;
		while (m->type == SnMethodTypeReference) {
			m = m->reference;
		}
		return m;
	}
	
	const SnMethod* snow_find_and_resolve_method(SnClass* cls, SnSymbol name) {
		return snow_resolve_method_reference(snow_find_method(cls, name));
	}
}

static VALUE class_inspect(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnClassType);
	SnClass* cls = (SnClass*)self;
	return snow_string_format("[Class:%s@%p]", cls->name ? snow_sym_to_cstr(cls->name) : "<anonymous>", self);
}

static VALUE class_call(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnClassType);
	SnClass* cls = (SnClass*)self;
	return snow_create_object(cls);
}

CAPI SnClass* snow_get_class_class() {
	static VALUE* root = NULL;
	if (!root) {
		SnMethod methods[] = {
			SN_METHOD("inspect", class_inspect, 0),
			SN_METHOD("to_string", class_inspect, 0),
			SN_METHOD("__call__", class_call, -1),
		};
		
		SnClass* cls = snow_define_class(snow_sym("Class"), NULL, 0, NULL, countof(methods), methods);
		cls->internal_type = SnClassType;
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}