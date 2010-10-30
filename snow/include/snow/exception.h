#pragma once
#ifndef EXCEPTION_H_WC3FV11H
#define EXCEPTION_H_WC3FV11H

#include "snow/basic.h"
#include "snow/value.h"

CAPI void snow_throw_exception(VALUE ex);
CAPI void snow_throw_exception_with_description(const char* fmt, ...);

#endif /* end of include guard: EXCEPTION_H_WC3FV11H */
