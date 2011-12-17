#pragma once
#ifndef CONTINUATION_H_6NGEZ11
#define CONTINUATION_H_6NGEZ11

#include "snow/object.h"

struct SnCallFrame;

CAPI SnInternalType SnFiberType;

CAPI SnObject* snow_create_fiber(VALUE functor); // functor is called with arguments calling_fiber, incoming_value
CAPI VALUE snow_fiber_resume(SnObject* fiber, VALUE incoming_value);
CAPI SnObject* snow_get_current_fiber();
CAPI SnObject* snow_fiber_get_link(const SnObject* fiber);
CAPI struct SnCallFrame* snow_fiber_get_current_frame(const SnObject* fiber);
CAPI struct SnObject* snow_get_fiber_class();

#endif /* end of include guard: CONTINUATION_H_6NGEZ11 */
