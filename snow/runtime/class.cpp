#include "snow/class.h"
#include "snow/function.h"
#include "snow/exception.h"
#include "snow/str.h"
#include "snow/snow.h"

#include "util.hpp"
#include "objectptr.hpp"
#include "internal.h"

#include <algorithm>
#include <vector>

namespace {
	using namespace snow;
	
	struct Class {
		static const SnInternalType* Type;
		
		SnSymbol name;
		const SnInternalType* instance_type;
		SnObject* super;
		std::vector<SnMethod> methods;
		std::vector<SnSymbol> instance_variables;
		VALUE initialize;
		bool is_meta;
		
		Class() : name(0), instance_type(NULL), super(NULL), is_meta(false) {}
		~Class() {
			for (size_t i = 0; i < methods.size(); ++i) {
				if (methods[i].type == SnMethodTypeProperty) {
					delete methods[i].property;
				}
			}
		}
	};
	const SnInternalType* Class::Type = &SnClassType;
	
	struct MethodLessThan {
		bool operator()(const SnMethod& a, const SnMethod& b) {
			return a.name < b.name;
		}
	};

	void class_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		#define CALLBACK(ROOT) callback(reinterpret_cast<VALUE*>(&(ROOT)))
		Class* priv = (Class*)data;
		CALLBACK(priv->super);
		for (size_t i = 0; i < priv->methods.size(); ++i) {
			switch (priv->methods[i].type) {
				case SnMethodTypeFunction: {
					CALLBACK(priv->methods[i].function);
					break;
				}
				case SnMethodTypeProperty: {
					CALLBACK(priv->methods[i].property->getter);
					CALLBACK(priv->methods[i].property->setter);
					break;
				}
				default: break;
			}
		}
	}
	
	VALUE class_initialize(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Class> cls = self;
		ASSERT(cls != NULL);
		cls->initialize = NULL;
		ObjectPtr<Class> super = it;
		if (super != NULL) {
			cls->super = super;
			cls->instance_type = super->instance_type;
			cls->instance_variables = super->instance_variables;
		} else if (snow_eval_truth(it)) {
			snow_throw_exception_with_description("Cannot use %p as superclass.", it);
		}
		return self;
	}
	
	VALUE class_define_method(const SnCallFrame* here, VALUE self, VALUE it) {
		ObjectPtr<Class> cls = self;
		if (cls != NULL) {
			if (here->args->size < 2) {
				snow_throw_exception_with_description("Class#define_method expects 2 arguments, %u given,", (uint32_t)here->args->size);
			}
			VALUE vname = it;
			VALUE vmethod = here->args->data[1];
			if (!snow_is_symbol(vname)) snow_throw_exception_with_description("Expected method name as argument 1 of Class#define_method.");
			_snow_class_define_method((SnObject*)self, snow_value_to_symbol(vname), vmethod);
			return self;
		}
		snow_throw_exception_with_description("Class#define_method called on an object that isn't a class: %p.", self);
		return NULL; // unreachable
	}
	
	VALUE class_define_property(const SnCallFrame* here, VALUE self, VALUE it) {
		// TODO: Use named arguments
		ObjectPtr<Class> cls = self;
		if (cls != NULL) {
			if (here->args->size < 2) {
				snow_throw_exception_with_description("Class#define_property expects 2 arguments, %u given,", (uint32_t)here->args->size);
			}
			VALUE vname = it;
			VALUE vgetter = here->args->data[1];
			VALUE vsetter = here->args->size >= 3 ? here->args->data[2] : NULL;
			if (!snow_is_symbol(vname)) snow_throw_exception_with_description("Class#define_property expects a property name as a symbol for the first argument.");
			_snow_class_define_property((SnObject*)self, snow_value_to_symbol(vname), vgetter, vsetter);
			return self;
		}
		snow_throw_exception_with_description("Class#define_property called on an object that isn't a class: %p.", self);
		return NULL; // unreachable
	}
	
	VALUE class_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
		if (!snow_is_class(self)) snow_throw_exception_with_description("Class#inspect called for object that is not a class: %p.", self);
		return snow_string_format("[Class@%p name:%s]", self, snow_class_get_name((SnObject*)self));
	}
	
	VALUE class_get_instance_methods(const SnCallFrame* here, VALUE self, VALUE it) {
		return NULL;
	}
	
	static VALUE class_call(const SnCallFrame* here, VALUE self, VALUE it) {
		SN_CHECK_CLASS(self, Class, __call__);
		return snow_create_object_with_arguments((SnObject*)self, here->args);
	}
}


CAPI {
	SnInternalType SnClassType = {
		.data_size = sizeof(Class),
		.initialize = snow::construct<Class>,
		.finalize = snow::destruct<Class>,
		.copy = snow::assign<Class>,
		.gc_each_root = class_gc_each_root,
	};
	
	SnObject* snow_create_object_without_initialize(SnObject* klass) {
		ObjectPtr<Class> cls = klass;
		ASSERT(cls != NULL); // klass is not a class!
		SnObject* obj = snow_gc_allocate_object(cls->instance_type);
		obj->cls = cls;
		obj->type = cls->instance_type;
		obj->num_alloc_members = 0;
		obj->members = NULL;
		return obj;
	}
	
	SnObject* snow_create_object(SnObject* cls, size_t num_constructor_args, VALUE* args) {
		SnArguments arguments = {
			.size = num_constructor_args,
			.data = args,
		};
		return snow_create_object_with_arguments(cls, &arguments);
	}
	
	SnObject* snow_create_object_with_arguments(SnObject* cls, const SnArguments* args) {
		SnObject* obj = snow_create_object_without_initialize(cls);
		// TODO: Consider how to call superclass initializers
		VALUE initialize = snow_class_get_initialize(cls);
		if (initialize) {
			snow_call_with_arguments(initialize, obj, args);
		}
		return obj;
	}
	
	bool snow_is_class(VALUE val) {
		return snow::value_is_of_type(val, SnClassType);
	}
	
	const char* snow_class_get_name(const SnObject* cls) {
		ObjectPtr<const Class> c = cls;
		return snow_sym_to_cstr(c->name);
	}
	
	bool snow_class_is_meta(const SnObject* cls) {
		ObjectPtr<const Class> c = cls;
		return c->is_meta;
	}
	
	int32_t snow_class_get_index_of_instance_variable(const SnObject* cls, SnSymbol name) {
		ObjectPtr<const Class> c = cls;
		ASSERT(c != NULL);
		for (size_t i = 0; i < c->instance_variables.size(); ++i) {
			if (c->instance_variables[i] == name) return (int32_t)i;
		}
		return -1;
	}
	
	int32_t snow_class_define_instance_variable(SnObject* cls, SnSymbol name) {
		ObjectPtr<Class> c = cls;
		ASSERT(snow_class_get_index_of_instance_variable(cls, name) < 0);
		int32_t i = (int32_t)c->instance_variables.size();
		c->instance_variables.push_back(name);
		return i;
	}
	
	size_t snow_class_get_num_instance_variables(const SnObject* cls) {
		return ObjectPtr<const Class>(cls)->instance_variables.size();
	}
	
	bool snow_class_lookup_method(const SnObject* cls, SnSymbol name, SnMethod* out_method) {
		const SnObject* object_class = snow_get_object_class();
		static const SnSymbol init_sym = snow_sym("initialize");
		SnMethod key = { .name = name, .type = SnMethodTypeNone };
		const SnObject* c = cls;
		while (c != NULL) {
			const Class* priv = snow::object_get_private<Class>(c, SnClassType);
			ASSERT(priv); // non-class in class hierarchy!
			
			// Check for 'initialize'
			if (name == init_sym && priv->initialize) {
				out_method->type = SnMethodTypeFunction;
				out_method->name = init_sym;
				out_method->function = priv->initialize;
				return true;
			}
			
			// Check regular methods
			std::vector<SnMethod>::const_iterator x = std::lower_bound(priv->methods.begin(), priv->methods.end(), key, MethodLessThan());
			if (x != priv->methods.end() && x->name == name) {
				*out_method = *x;
				return true;
			}
			
			if (c != object_class) {
				c = priv->super ? priv->super : object_class;
			} else {
				return false;
			}
		}
		return false;
	}
	
	void snow_class_get_method(const SnObject* cls, SnSymbol name, SnMethod* out_method) {
		if (!snow_class_lookup_method(cls, name, out_method)) {
			snow_throw_exception_with_description("Class %s@%p does not contain a method or property named '%s'.", snow_class_get_name(cls), cls, snow_sym_to_cstr(name));
		}
	}
	
	VALUE snow_class_get_initialize(const SnObject* cls) {
		return ObjectPtr<const Class>(cls)->initialize;
	}
	
	static bool class_define_method_or_property(SnObject* cls, const SnMethod& key) {
		ObjectPtr<Class> c = cls;
		ASSERT(c != NULL); // cls is not a class!
		
		static const SnSymbol init_sym = snow_sym("initialize");
		if (key.name == init_sym) {
			if (key.type == SnMethodTypeProperty)
				snow_throw_exception_with_description("Cannot define a property by the name 'initialize'.");
			c->initialize = key.function;
		}

		std::vector<SnMethod>::iterator x = std::lower_bound(c->methods.begin(), c->methods.end(), key, MethodLessThan());
		if (x == c->methods.end() || x->name != key.name) {
			c->methods.insert(x, key);
			return true;
		}
		return false;
	}
	
	SnObject* _snow_class_define_method(SnObject* cls, SnSymbol name, VALUE function) {
		SnMethod key = { .name = name, .type = SnMethodTypeFunction, .function = function };
		if (!class_define_method_or_property(cls, key)) {
			snow_throw_exception_with_description("Method or property '%s' is already defined in class %s@%p.", snow_sym_to_cstr(name), snow_class_get_name(cls), cls);
		}
		return cls;
	}
	
	SnObject* _snow_class_define_property(SnObject* cls, SnSymbol name, VALUE getter, VALUE setter) {
		SnProperty* property = new SnProperty;
		property->getter = getter;
		property->setter = setter;
		SnMethod key = { .name = name, .type = SnMethodTypeProperty, .property = property };
		if (!class_define_method_or_property(cls, key)) {
			delete property;
			snow_throw_exception_with_description("Method or property '%s' is already defined in class %s@%p.", snow_sym_to_cstr(name), snow_class_get_name(cls), cls);
		}
		return cls;
	}
	
	SnObject* snow_get_class_class() {
		static SnObject** root = NULL;
		if (!root) {
			// bootstrapping
			SnObject* class_class = snow_gc_allocate_object(&SnClassType);
			class_class->cls = class_class;
			Class* priv = snow::object_get_private<Class>(class_class, SnClassType);
			priv->instance_type = &SnClassType;
			root = snow_gc_create_root(class_class);
			
			snow_class_define_method(class_class, "initialize", class_initialize);
			snow_class_define_method(class_class, "define_method", class_define_method);
			snow_class_define_method(class_class, "define_property", class_define_property);
			snow_class_define_method(class_class, "inspect", class_inspect);
			snow_class_define_method(class_class, "to_string", class_inspect);
			snow_class_define_method(class_class, "__call__", class_call);
			snow_class_define_property(class_class, "instance_methods", class_get_instance_methods, NULL);
		}
		return *root;
	}
	
	SnObject* snow_create_class(SnSymbol name, SnObject* super) {
		VALUE args[] = { super };
		ObjectPtr<Class> cls = snow_create_object(snow_get_class_class(), countof(args), args);
		cls->name = name;
		cls->initialize = NULL;
		cls->is_meta = false;
		return cls;
	}
	
	SnObject* snow_create_class_for_type(SnSymbol name, const SnInternalType* type) {
		ObjectPtr<Class> cls = snow_create_object_without_initialize(snow_get_class_class());
		cls->name = name;
		cls->instance_type = type;
		cls->super = NULL;
		cls->initialize = NULL;
		cls->is_meta = false;
		return cls;
	}
	
	SnObject* snow_create_meta_class(SnObject* super) {
		ASSERT(snow_is_class(super));
		VALUE args[] = { super };
		ObjectPtr<Class> cls = snow_create_object(snow_get_class_class(), countof(args), args);
		ObjectPtr<Class> sup = super;
		cls->name = sup->name;
		cls->is_meta = true;
		return cls;
	}
}