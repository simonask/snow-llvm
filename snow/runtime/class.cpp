#include "snow/class.h"
#include "snow/function.h"
#include "snow/str.h"
#include "snow/exception.h"
#include "internal.h"
#include "linkheap.hpp"
#include "util.hpp"

#include <algorithm>

namespace {
	struct MethodComparer {
		bool operator()(const SnMethod& a, const SnMethod& b) {
			return a.name < b.name;
		}
	};
}

CAPI {
	using namespace snow;
	
	SnClass* snow_create_class(SnSymbol name, SnClass* super) {
		SnClass* cls = SN_GC_ALLOC_OBJECT(SnClass);
		_snow_object_init(&cls->base, snow_get_class_class());
		cls->name = name;
		cls->super = super;
		cls->is_meta = false;
		if (!super) {
			cls->internal_type = SnObjectType;
			cls->fields = NULL;
			cls->methods = NULL;
			cls->num_fields = 0;
			cls->num_methods = 0;
		} else {
			cls->internal_type = super->internal_type;
			cls->fields = super->fields ? alloc_range<SnField>(super->num_fields) : NULL;
			copy_construct_range(cls->fields, super->fields, super->num_fields);
			cls->num_fields = super->num_fields;
			cls->methods = NULL;
			cls->num_methods = 0;
		}
		return cls;
	}
	
	void snow_finalize_class(SnClass* cls) {
		free(cls->fields);
		free(cls->methods);
	}
	
	SnClass* snow_create_meta_class(SnClass* base) {
		SnClass* meta = snow_create_class(base->name, base);
		meta->is_meta = true;
		return meta;
	}
	
	bool _snow_class_define_method(SnClass* cls, SnSymbol name, VALUE func) {
		SnMethod key = { name, SnMethodTypeFunction, .function = func };
		SnMethod* insertion_point = std::lower_bound(cls->methods, cls->methods + cls->num_methods, key, MethodComparer());
		if (insertion_point && insertion_point->name == name)
			return false; // already defined
		int32_t insertion_index = insertion_point - cls->methods;
		cls->methods = realloc_range<SnMethod>(cls->methods, cls->num_methods, cls->num_methods+1);
		cls->num_methods += 1;
		for (int32_t i = cls->num_methods-1; i >= insertion_index; --i) {
			cls->methods[i] = cls->methods[i-1];
		}
		cls->methods[insertion_index] = key;
		return true;
	}
	
	bool _snow_class_define_property(SnClass* cls, SnSymbol name, VALUE getter, VALUE setter) {
		return false; // XXX: TODO
	}
	
	VALUE snow_class_get_method(const SnClass* cls, SnSymbol name) {
		const SnClass* c = cls;
		const SnClass* object_class = snow_get_object_class();
		SnMethod key = { name, SnMethodTypeFunction, .function = NULL };
		while (c != NULL) {
			const SnMethod* begin = c->methods;
			const SnMethod* end = c->methods + c->num_methods;
			const SnMethod* x = std::lower_bound(begin, end, key, MethodComparer());
			if (x != end && x->name == name) {
				return x->function;
			}
			
			if (c != object_class) {
				c = c->super ? c->super : object_class;
			} else {
				c = NULL;
			}
		}
		snow_throw_exception_with_description("Method '%s' not found on class %s@%p.\n", snow_sym_to_cstr(name), snow_sym_to_cstr(cls->name), cls);
		return NULL;
	}
	
	int32_t snow_class_get_index_of_field(const SnClass* cls, SnSymbol name) {
		for (int32_t i = 0; i < cls->num_fields; ++i) {
			if (cls->fields[i].name == name) return i;
		}
		return -1;
	}
	
	int32_t snow_class_get_or_define_index_of_field(SnClass* cls, SnSymbol name) {
		int32_t idx = snow_class_get_index_of_field(cls, name);
		if (idx < 0) {
			cls->fields = realloc_range<SnField>(cls->fields, cls->num_fields, cls->num_fields+1);
			idx = cls->num_fields++;
			cls->fields[idx].name = name;
			cls->fields[idx].flags = SnFieldNoFlags;
		}
		return idx;
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

static VALUE class_define_method(SnFunction* function, SnCallFrame* here, VALUE self, VALUE it) {
	ASSERT(snow_type_of(self) == SnClassType);
	ASSERT(snow_type_of(it) == SnSymbolType);
	SnSymbol name = snow_value_to_symbol(it);
	VALUE method = snow_arguments_get_by_index(here->arguments, 1);
	SnClass* cls = (SnClass*)self;
	if (!_snow_class_define_method(cls, name, method)) {
		snow_throw_exception_with_description("Method '%s' is already defined for class %s@%p.", snow_sym_to_cstr(name), snow_sym_to_cstr(cls->name), cls);
	}
	return self;
}

CAPI SnClass* snow_get_class_class() {
	static VALUE* root = NULL;
	if (!root) {
		// We cannot use snow_define_class, since that would cause an infinite loop, so do it manually.
		SnClass* cls = SN_GC_ALLOC_OBJECT(SnClass);
		cls->is_meta = false;
		cls->internal_type = SnClassType;
		cls->name = snow_sym("Class");
		cls->super = NULL;
		cls->fields = NULL;
		cls->methods = NULL;
		cls->num_fields = 0;
		cls->num_methods = 0;
		_snow_object_init(&cls->base, cls);
		
		snow_class_define_method(cls, "inspect", class_inspect, 0);
		snow_class_define_method(cls, "to_string", class_inspect, 0);
		snow_class_define_method(cls, "__call__", class_call, -1);
		snow_class_define_method(cls, "define_method", class_define_method, 2);
		
		root = snow_gc_create_root(cls);
	}
	return (SnClass*)*root;
}