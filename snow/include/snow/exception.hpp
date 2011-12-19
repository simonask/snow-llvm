#pragma once
#ifndef EXCEPTION_HPP_M1JVGOQ3
#define EXCEPTION_HPP_M1JVGOQ3

#include "snow/value.hpp"

namespace snow {
	struct Exception {
		VALUE value;
		Exception(VALUE v) : value(v) {}
	};
	
	VALUE try_catch_ensure(VALUE try_f, VALUE catch_f, VALUE ensure_f);
	void throw_exception(VALUE ex);
	void throw_exception_with_description(const char* fmt, ...);
}

#endif /* end of include guard: EXCEPTION_HPP_M1JVGOQ3 */
