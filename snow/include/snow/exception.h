#pragma once
#ifndef EXCEPTION_H_WC3FV11H
#define EXCEPTION_H_WC3FV11H

#include "snow/basic.h"
#include "snow/value.h"

CAPI void snow_throw_exception(SN_P, VALUE ex);
CAPI void snow_throw_exception_with_description(SN_P, const char* fmt, ...);

#endif /* end of include guard: EXCEPTION_H_WC3FV11H */
