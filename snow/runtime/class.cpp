#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include "snow/str-format.hpp"

#include "snow/util.hpp"
#include "snow/objectptr.hpp"
#include "internal.h"

#include <algorithm>
#include <vector>

namespace snow {
	struct Class {
		Symbol name;
		const Type* instance_type;
		ObjectPtr<Class> super;
		std::vector<Method> methods;
		std::vector<Symbol> instance_variables;
		Value initialize;
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
	
	static void class_gc_each_root(void* priv, GCCallback callback) {
		auto cls = static_cast<Class*>(priv);
		callback(cls->super);
		for (auto it = cls->methods.begin(); it != cls->methods.end(); ++it) {
			if (it->type == MethodTypeFunction) {
				callback(it->function);
			} else {
				callback(it->property->getter);
				callback(it->property->setter);
			}
		}
		// cls->initialize is also in the method list, so already checked by now.
	}
	
	SN_REGISTER_CPP_TYPE(Class, class_gc_each_root)
	
	struct MethodLessThan {
		bool operator()(const Method& a, const Method& b) {
			return a.name < b.name;
		}
	};

	namespace bindings {
		VALUE class_initialize(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Class> cls = self;
			ASSERT(cls != NULL);
			cls->initialize = NULL;
			ObjectPtr<Class> super = it;
			if (super != NULL) {
				cls->super = super;
				cls->instance_type = super->instance_type;
				cls->instance_variables = super->instance_variables;
			} else if (is_truthy(it)) {
				throw_exception_with_description("Cannot use %@ as superclass.", it);
			}
			return self;
		}

		VALUE class_define_method(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<Class> cls = self;
			if (cls != NULL) {
				if (here->args->size < 2) {
					throw_exception_with_description("Class#define_method expects 2 arguments, %@ given,", here->args->size);
				}
				Value vmethod = here->args->data[1];
				if (!is_symbol(it)) throw_exception_with_description("Expected method name as first argument of Class#define_method.");
				_class_define_method(cls, value_to_symbol(it), vmethod);
				return self;
			}
			throw_exception_with_description("Class#define_method called on an object that isn't a class: %@.", value_inspect(self));
			return NULL; // unreachable
		}

		VALUE class_define_property(const CallFrame* here, VALUE self, VALUE it) {
			// TODO: Use named arguments
			ObjectPtr<Class> cls = self;
			if (cls != NULL) {
				if (here->args->size < 2) {
					throw_exception_with_description("Class#define_property expects 2 arguments, %@ given,", here->args->size);
				}
				Value vgetter = here->args->data[1];
				Value vsetter = here->args->size >= 3 ? here->args->data[2] : NULL;
				if (!is_symbol(it)) throw_exception_with_description("Class#define_property expects a property name as a symbol for the first argument.");
				_class_define_property(cls, value_to_symbol(it), vgetter, vsetter);
				return self;
			}
			throw_exception_with_description("Class#define_property called on an object that isn't a class: %@.", value_inspect(self));
			return NULL; // unreachable
		}

		VALUE class_inspect(const CallFrame* here, VALUE self, VALUE it) {
			if (!is_class(self)) throw_exception_with_description("Class#inspect called for object that is not a class: %@.", value_inspect(self));
			return format_string("[Class@%@ name:%@]", format::pointer(self), class_get_name(self));
		}

		VALUE class_get_instance_methods(const CallFrame* here, VALUE self, VALUE it) {
			return NULL;
		}

		static VALUE class_call(const CallFrame* here, VALUE self, VALUE it) {
			return create_object_with_arguments(self, here->args);
		}
	}

	
	bool is_class(Value val) {
		return value_is_of_type(val, get_type<Class>());
	}
	
	const char* class_get_name(ClassConstPtr cls) {
		return snow::sym_to_cstr(cls->name);
	}
	
	bool class_is_meta(ClassConstPtr cls) {
		return cls->is_meta;
	}
	
	int32_t class_get_index_of_instance_variable(ClassConstPtr cls, Symbol name) {
		for (size_t i = 0; i < cls->instance_variables.size(); ++i) {
			if (cls->instance_variables[i] == name) return (int32_t)i;
		}
		return -1;
	}
	
	int32_t class_define_instance_variable(ClassPtr cls, Symbol name) {
		ASSERT(class_get_index_of_instance_variable(cls, name) < 0); // variable already defined
		int32_t i = (int32_t)cls->instance_variables.size();
		cls->instance_variables.push_back(name);
		return i;
	}
	
	size_t class_get_num_instance_variables(ClassConstPtr cls) {
		return cls->instance_variables.size();
	}
	
	static bool class_lookup_method(ClassConstPtr cls, Symbol name, Method* out_method) {
		ObjectPtr<const Class> object_class = get_object_class();
		static const Symbol init_sym = snow::sym("initialize");
		Method key = { .name = name, .type = MethodTypeNone };
		ObjectPtr<const Class> c = cls;
		while (c != NULL) {
			// Check for 'initialize'
			if (name == init_sym && c->initialize) {
				out_method->type = MethodTypeFunction;
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
				if (c->super != NULL) {
					c = c->super;
				} else {
					c = object_class;
				}
			} else {
				return false;
			}
		}
		return false;
	}
	
	bool class_lookup_method(ClassConstPtr cls, Symbol name, MethodQueryResult* out_method) {
		Method m;
		if (class_lookup_method(cls, name, &m)) {
			out_method->type = m.type;
			out_method->result = m.type == MethodTypeFunction ? m.function : m.property->getter;
			return true;
		}
		
		if (class_lookup_method(cls, sym("method_missing"), &m)) {
			out_method->type = MethodTypeMissing;
			out_method->result = m.function;
			return true;
		}
		
		TRAP(); // method_missing not defined in Object.
		return false;
	}
	
	bool class_lookup_property_setter(ClassConstPtr cls, Symbol name, MethodQueryResult* out_method) {
		Method m;
		if (class_lookup_method(cls, name, &m)) {
			if (m.type == MethodTypeProperty) {
				out_method->type = m.type;
				out_method->result = m.property->setter;
				return true;
			}
		}
		return false;
	}
	
	Value class_get_initialize(ClassConstPtr cls) {
		return cls->initialize;
	}
	
	static bool class_define_method_or_property(ClassPtr cls, const Method& key) {
		static const Symbol init_sym = snow::sym("initialize");
		if (key.name == init_sym) {
			if (key.type == MethodTypeProperty)
				throw_exception_with_description("Cannot define a property by the name 'initialize'.");
			cls->initialize = key.function;
		}

		std::vector<Method>::iterator x = std::lower_bound(cls->methods.begin(), cls->methods.end(), key, MethodLessThan());
		if (x == cls->methods.end() || x->name != key.name) {
			cls->methods.insert(x, key);
			return true;
		}
		return false;
	}
	
	ClassPtr _class_define_method(ClassPtr cls, Symbol name, Value function) {
		ASSERT(!snow::is_symbol(function));
		Method key = { .name = name, .type = MethodTypeFunction, .function = function, .property = NULL };
		if (!class_define_method_or_property(cls, key)) {
			throw_exception_with_description("Method or property '%@' is already defined in class %@@%@.", snow::sym_to_cstr(name), class_get_name(cls), format::pointer(cls));
		}
		return cls;
	}
	
	ClassPtr _class_define_property(ClassPtr cls, Symbol name, Value getter, Value setter) {
		Property* property = new Property;
		property->getter = getter;
		property->setter = setter;
		Method key = { .name = name, .type = MethodTypeProperty, .function = NULL, .property = property };
		if (!class_define_method_or_property(cls, key)) {
			delete property;
			throw_exception_with_description("Method or property '%@' is already defined in class %@@%@.", snow::sym_to_cstr(name), class_get_name(cls), format::pointer(cls));
		}
		return cls;
	}
	
	ObjectPtr<Class> get_class_class() {
		static Value* root = NULL;
		if (!root) {
			// bootstrapping
			AnyObjectPtr class_class = gc_allocate_object(get_type<Class>());
			class_class->cls = class_class;
			Class* priv = snow::object_get_private<Class>(class_class);
			priv->instance_type = get_type<Class>();
			root = gc_create_root(class_class);
			
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
	
	ObjectPtr<Class> create_class(Symbol name, ClassPtr super) {
		Value args[] = { super };
		ObjectPtr<Class> cls = create_object(get_class_class(), countof(args), args);
		cls->name = name;
		cls->initialize = NULL;
		cls->is_meta = false;
		return cls;
	}
	
	ObjectPtr<Class> create_class_for_type(Symbol name, const Type* type) {
		ObjectPtr<Class> cls = create_object_without_initialize(get_class_class());
		cls->name = name;
		cls->instance_type = type;
		cls->super = NULL;
		cls->initialize = NULL;
		cls->is_meta = false;
		return cls;
	}
	
	ObjectPtr<Class> create_meta_class(ClassPtr super) {
		ASSERT(is_class(super));
		Value args[] = { super };
		ObjectPtr<Class> cls = create_object(get_class_class(), countof(args), args);
		ObjectPtr<Class> sup = super;
		cls->name = sup->name;
		cls->is_meta = true;
		return cls;
	}

	AnyObjectPtr create_object_without_initialize(ClassPtr cls) {
		ASSERT(cls != NULL); // cls is not a class!
		Object* obj = gc_allocate_object(cls->instance_type);
		obj->cls = cls;
		obj->type = cls->instance_type;
		obj->num_alloc_members = 0;
		obj->members = NULL;
		return obj;
	}
	
	void object_initialize(AnyObjectPtr object, const SnArguments* args) {
		// TODO: Consider how to call superclass initializers
		Value initialize = class_get_initialize(get_class(object));
		if (initialize != NULL) {
			call_with_arguments(initialize, object, args);
		}
	}
	
	AnyObjectPtr create_object(ClassPtr cls, size_t num_constructor_args, const Value* args) {
		SnArguments arguments = {
			.size = num_constructor_args,
			.data = args,
		};
		return create_object_with_arguments(cls, &arguments);
	}
	
	AnyObjectPtr create_object_with_arguments(ClassPtr cls, const SnArguments* args) {
		AnyObjectPtr obj = create_object_without_initialize(cls);
		object_initialize(obj, args);
		return obj;
	}
}