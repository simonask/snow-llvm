#pragma once
#ifndef EXCEPTION_HPP_M1JVGOQ3
#define EXCEPTION_HPP_M1JVGOQ3

#include "snow/value.hpp"
#include "snow/str-format.hpp"

namespace snow {
	struct Exception {
		Value value;
		Exception(Value v) : value(v) {}
	};
	
	Value try_catch_ensure(Value try_f, Value catch_f, Value ensure_f);
	void throw_exception(Value ex);
	
	template <typename... Args>
	void throw_exception_with_description(const char* fmt, Args... args) {
		ObjectPtr<String> description = format_string(fmt, args...);
		throw_exception(description);
	}
}

#endif /* end of include guard: EXCEPTION_HPP_M1JVGOQ3 */
