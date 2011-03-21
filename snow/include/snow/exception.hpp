#pragma once
#ifndef EXCEPTION_HPP_M1JVGOQ3
#define EXCEPTION_HPP_M1JVGOQ3

#include "snow/exception.h"

namespace snow {
	struct Exception {
		VALUE value;
		Exception(VALUE v) : value(v) {}
	};
}

#endif /* end of include guard: EXCEPTION_HPP_M1JVGOQ3 */
