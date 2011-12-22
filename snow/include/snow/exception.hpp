#pragma once
#ifndef EXCEPTION_HPP_M1JVGOQ3
#define EXCEPTION_HPP_M1JVGOQ3

#include "snow/value.hpp"

namespace snow {
	struct Exception {
		Value value;
		Exception(const Value& v) : value(v) {}
	};
	
	Value try_catch_ensure(const Value& try_f, const Value& catch_f, const Value& ensure_f);
	void throw_exception(const Value& ex);
	void throw_exception_with_description(const char* fmt, ...);
}

#endif /* end of include guard: EXCEPTION_HPP_M1JVGOQ3 */
