#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/exception.h"
#include "snow/str.hpp"
#include "snow/snow.hpp"

#include "snow/util.hpp"
#include "snow/objectptr.hpp"
#include "internal.h"

#include <algorithm>
#include <vector>

namespace snow {
	struct Class {
		SnSymbol name;
		const Type* instance_type;
		ObjectPtr<Class> super;
		std::vector<Method> methods;
		std::vector<SnSymbol> instance_variables;
		VALUE initialize;
		bool is_meta;
		
		Class() : name(0), instance_type(NULL), is_meta(false) {}
		~Class() {
			for (size_t i = 0; i < methods.size(); ++i) {
				if (methods[i].type == MethodTypeProperty) {
					delete methods[i].property;
				}
			}
		}
	};

	using namespace snow;
	
	struct MethodLessThan {
		bool operator()(const Method& a, const Method& b) {
			return a.name < b.name;
		}
	};

	void class_gc_each_root(void* data, void(*callback)(VALUE* root)) {
		#define CALLBACK(ROOT) callback(reinterpret_cast<VALUE*>(&(ROOT)))
		Class* priv = (Class*)data;
		CALLBACK(priv->super);
		for (size_t i = 0; i < priv->methods.size(); ++i) {
			switch (priv->methods[i].type) {
				case MethodTypeFunction: {
					CALLBACK(priv->methods[i].function);
					break;
				}
				case MethodTypeProperty: {
					CALLBACK(priv->methods[i].property->getter);
					CALLBACK(priv->methods[i].property->setter);
					break;
				}
				default: break;
			}
		}
	}
	
	SN_REGISTER_CPP_TYPE(Class, class_gc_each_root)

	namespace bindings {
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
				_class_define_method(cls, snow_value_to_symbol(vname), vmethod);
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
				_class_define_property(cls, snow_value_to_symbol(vname), vgetter, vsetter);
				return self;
			}
			snow_throw_exception_with_description("Class#define_property called on an object that isn't a class: %p.", self);
			return NULL; // unreachable
		}

		VALUE class_inspect(const SnCallFrame* here, VALUE self, VALUE it) {
			if (!is_class(self)) snow_throw_exception_with_description("Class#inspect called for object that is not a class: %p.", self);
			return string_format("[Class@%p name:%s]", self, class_get_name((SnObject*)self));
		}

		VALUE class_get_instance_methods(const SnCallFrame* here, VALUE self, VALUE it) {
			return NULL;
		}

		static VALUE class_call(const SnCallFrame* here, VALUE self, VALUE it) {
			SN_CHECK_CLASS(self, Class, __call__);
			return snow_create_object_with_arguments((SnObject*)self, here->args);
		}
	}

	
	bool is_class(VALUE val) {
		return value_is_of_type(val, get_type<Class>());
	}
	
	const char* class_get_name(ClassConstPtr cls) {
		return snow_sym_to_cstr(cls->name);
	}
	
	bool class_is_meta(ClassConstPtr cls) {
		return cls->is_meta;
	}
	
	int32_t class_get_index_of_instance_variable(ClassConstPtr cls, SnSymbol name) {
		for (size_t i = 0; i < cls->instance_variables.size(); ++i) {
			if (cls->instance_variables[i] == name) return (int32_t)i;
		}
		return -1;
	}
	
	int32_t class_define_instance_variable(ClassPtr cls, SnSymbol name) {
		ASSERT(class_get_index_of_instance_variable(cls, name) < 0); // variable already defined
		int32_t i = (int32_t)cls->instance_variables.size();
		cls->instance_variables.push_back(name);
		return i;
	}
	
	size_t class_get_num_instance_variables(ClassConstPtr cls) {
		return cls->instance_variables.size();
	}
	
	bool class_lookup_method(ClassConstPtr cls, SnSymbol name, Method* out_method) {
		ObjectPtr<const Class> object_class = snow_get_object_class();
		static const SnSymbol init_sym = snow_sym("initialize");
		Method key = { .name = name, .type = MethodTypeNone };
		ObjectPtr<const Class> c = cls;
		while (c != NULL) {
			// Check for 'initialize'
			if (name == init_sym && c->initialize) {
				out_method->type = MethodTypeFunction;
				out_method->name = init_sym;
				out_method->function = c->initialize;
				return true;
			}
			
			// Check regular methods
			std::vector<Method>::const_iterator x = std::lower_bound(c->methods.begin(), c->methods.end(), key, MethodLessThan());
			if (x != c->methods.end() && x->name == name) {
				*out_method = *x;
				return true;
			}
			
			if (c != object_class) {
				c = c->super ? c->super : object_class;
			} else {
				return false;
			}
		}
		return false;
	}
	
	void class_get_method(ClassConstPtr cls, SnSymbol name, Method* out_method) {
		if (!class_lookup_method(cls, name, out_method)) {
			snow_throw_exception_with_description("Class %s@%p does not contain a method or property named '%s'.", class_get_name(cls), cls.value(), snow_sym_to_cstr(name));
		}
	}
	
	VALUE class_get_initialize(ClassConstPtr cls) {
		return cls->initialize;
	}
	
	static bool class_define_method_or_property(ClassPtr cls, const Method& key) {
		static const SnSymbol init_sym = snow_sym("initialize");
		if (key.name == init_sym) {
			if (key.type == MethodTypeProperty)
				snow_throw_exception_with_description("Cannot define a property by the name 'initialize'.");
			cls->initialize = key.function;
		}

		std::vector<Method>::iterator x = std::lower_bound(cls->methods.begin(), cls->methods.end(), key, MethodLessThan());
		if (x == cls->methods.end() || x->name != key.name) {
			cls->methods.insert(x, key);
			return true;
		}
		return false;
	}
	
	ClassPtr _class_define_method(ClassPtr cls, SnSymbol name, VALUE function) {
		Method key = { .name = name, .type = MethodTypeFunction, .function = function };
		if (!class_define_method_or_property(cls, key)) {
			snow_throw_exception_with_description("Method or property '%s' is already defined in class %s@%p.", snow_sym_to_cstr(name), class_get_name(cls), (VALUE)cls);
		}
		return cls;
	}
	
	ClassPtr _class_define_property(ClassPtr cls, SnSymbol name, VALUE getter, VALUE setter) {
		Property* property = new Property;
		property->getter = getter;
		property->setter = setter;
		Method key = { .name = name, .type = MethodTypeProperty, .property = property };
		if (!class_define_method_or_property(cls, key)) {
			delete property;
			snow_throw_exception_with_description("Method or property '%s' is already defined in class %s@%p.", snow_sym_to_cstr(name), class_get_name(cls), (VALUE)cls);
		}
		return cls;
	}
	
	ObjectPtr<Class> get_class_class() {
		static SnObject** root = NULL;
		if (!root) {
			// bootstrapping
			SnObject* class_class = snow_gc_allocate_object(get_type<Class>());
			class_class->cls = class_class;
			Class* priv = snow::object_get_private<Class>(class_class, get_type<Class>());
			priv->instance_type = get_type<Class>();
			root = snow_gc_create_root(class_class);
			
			SN_DEFINE_METHOD(class_class, "initialize", bindings::class_initialize);
			SN_DEFINE_METHOD(class_class, "define_method", bindings::class_define_method);
			SN_DEFINE_METHOD(class_class, "define_property", bindings::class_define_property);
			SN_DEFINE_METHOD(class_class, "inspect", bindings::class_inspect);
			SN_DEFINE_METHOD(class_class, "to_string", bindings::class_inspect);
			SN_DEFINE_METHOD(class_class, "__call__", bindings::class_call);
			SN_DEFINE_PROPERTY(class_class, "instance_methods", bindings::class_get_instance_methods, NULL);
		}
		return *root;
	}
	
	ObjectPtr<Class> create_class(SnSymbol name, ClassPtr super) {
		VALUE args[] = { super };
		ObjectPtr<Class> cls = snow_create_object(get_class_class(), countof(args), args);
		cls->name = name;
		cls->initialize = NULL;
		cls->is_meta = false;
		return cls;
	}
	
	ObjectPtr<Class> create_class_for_type(SnSymbol name, const Type* type) {
		ObjectPtr<Class> cls = snow_create_object_without_initialize(get_class_class());
		cls->name = name;
		cls->instance_type = type;
		cls->super = NULL;
		cls->initialize = NULL;
		cls->is_meta = false;
		return cls;
	}
	
	ObjectPtr<Class> create_meta_class(ClassPtr super) {
		ASSERT(is_class(super));
		VALUE args[] = { super };
		ObjectPtr<Class> cls = snow_create_object(get_class_class(), countof(args), args);
		ObjectPtr<Class> sup = super;
		cls->name = sup->name;
		cls->is_meta = true;
		return cls;
	}
}

CAPI {
	using namespace snow;
	
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
		VALUE initialize = class_get_initialize(cls);
		if (initialize) {
			snow_call_with_arguments(initialize, obj, args);
		}
		return obj;
	}
}