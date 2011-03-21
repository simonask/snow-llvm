#pragma once
#ifndef EXCEPTION_H_WC3FV11H
#define EXCEPTION_H_WC3FV11H

#include "snow/basic.h"
#include "snow/value.h"

CAPI VALUE snow_try_catch_ensure(VALUE try_f, VALUE catch_f, VALUE ensure_f);
CAPI void snow_throw_exception(VALUE ex);
CAPI void snow_throw_exception_with_description(const char* fmt, ...);

#endif /* end of include guard: EXCEPTION_H_WC3FV11H */
