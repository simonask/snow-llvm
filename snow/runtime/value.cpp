#include "snow/value.hpp"
#include "snow/object.hpp"
#include "snow/gc.hpp"

namespace snow {
	void Value::assign(VALUE val) {
		if (is_object()) {
			Object* obj = (Object*)value_;
			--obj->refcount;
			if (obj->refcount == 0) {
				snow::gc_free_object(obj);
			}
		}
		value_ = val;
		if (is_object()) {
			Object* obj = (Object*)value_;
			++obj->refcount;
		}
	}
}