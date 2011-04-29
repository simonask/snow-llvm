#pragma once
#ifndef INTERNAL_H_MQQA0XIE
#define INTERNAL_H_MQQA0XIE

#include "snow/value.h"

struct SnObject;
struct SnClass;

CAPI void _snow_object_init(struct SnObject* obj, struct SnClass* cls);

#define countof(X) (sizeof(X)/sizeof(X[0]))

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) snow_printf(fmt, __VA_ARGS__, NULL)
#else
#define DEBUG_PRINT(...) 
#endif

#endif /* end of include guard: INTERNAL_H_MQQA0XIE */
