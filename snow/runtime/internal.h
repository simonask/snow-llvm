#pragma once
#ifndef INTERNAL_H_MQQA0XIE
#define INTERNAL_H_MQQA0XIE

#include "snow/value.hpp"

struct SnObject;

#define countof(X) (sizeof(X)/sizeof(X[0]))
#define UNSAFE_OFFSET_OF(TYPE, FIELD) (size_t)(&((const TYPE*)NULL)->FIELD)

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) snow_printf(fmt, __VA_ARGS__, NULL)
#else
#define DEBUG_PRINT(...) 
#endif

#if defined(__cplusplus)
#define SN_CHECK_CLASS(OBJECT, CLASS_NAME, METHOD_NAME) do{ if (ObjectPtr<CLASS_NAME>(OBJECT) == NULL) throw_exception_with_description(#CLASS_NAME "#" #METHOD_NAME " called on an object of the wrong type: %@.", OBJECT); } while(0)
#else
// TODO: Plain C version of SN_CHECK_CLASS
#endif

#endif /* end of include guard: INTERNAL_H_MQQA0XIE */
