#include "snow/nil.hpp"
#include "internal.h"
#include "snow/class.hpp"
#include "snow/str.hpp"
#include "snow/function.hpp"

namespace snow {
	namespace {
		static VALUE nil_inspect(const CallFrame* here, VALUE self, VALUE it) {
			return snow::create_string_constant("nil");
		}

		static VALUE nil_to_string(const CallFrame* here, VALUE self, VALUE it) {
			return snow::create_string_constant("");
		}
	}
	
	ObjectPtr<Class> get_nil_class() {
		static Value* root = NULL;
		if (!root) {
			ObjectPtr<Class> cls = create_class(snow::sym("Nil"), NULL);
			SN_DEFINE_METHOD(cls, "inspect", nil_inspect);
			SN_DEFINE_METHOD(cls, "to_string", nil_to_string);
			root = gc_create_root(cls);
		}
		return *root;
	}
	
}
