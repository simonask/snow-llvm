#include "snow/boolean.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/function.hpp"
#include "snow/str.hpp"

namespace snow {
	namespace bindings {
		static VALUE boolean_inspect(const CallFrame* here, VALUE self, VALUE it) {
			return snow::create_string_constant(value_to_boolean(self) ? "true" : "false");
		}

		static VALUE boolean_complement(const CallFrame* here, VALUE self, VALUE it) {
			return boolean_to_value(!value_to_boolean(self));
		}
	}
	
	
	ObjectPtr<Class> get_boolean_class() {
		static SnObject** root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class(snow::sym("Boolean"), NULL);
			SN_DEFINE_METHOD(cls, "inspect", bindings::boolean_inspect);
			SN_DEFINE_METHOD(cls, "to_string", bindings::boolean_inspect);
			SN_DEFINE_METHOD(cls, "~", bindings::boolean_complement);	
			root = gc_create_root(cls);
		}
		return *root;
	}
}
