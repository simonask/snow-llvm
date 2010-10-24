#pragma once
#ifndef SNOW_H_ILWGKE1Z
#define SNOW_H_ILWGKE1Z

#include "snow/basic.h"
#include "snow/value.h"
#include "snow/symbol.h"

struct SnArguments;
struct SnProcess;
struct SnVM;
struct SnObject;
struct SnType;

CAPI SN_P snow_init(struct SnVM*);
CAPI SN_P snow_get_main_process();
CAPI const struct SnType* snow_get_type(VALUE v);
CAPI void snow_set_global(SN_P, SnSymbol sym, VALUE val);
CAPI VALUE snow_get_global(SN_P, SnSymbol sym);
CAPI VALUE snow_require(SN_P, const char* str);
CAPI VALUE snow_eval(SN_P, const char* str);
CAPI VALUE snow_compile(SN_P, const char* str);
CAPI VALUE snow_compile_file(SN_P, const char* path);
CAPI VALUE snow_call(SN_P, VALUE object, struct SnArguments*);
CAPI VALUE snow_call_with_self(SN_P, VALUE object, VALUE self, struct SnArguments*);
CAPI VALUE snow_call_method(SN_P, VALUE self, SnSymbol member, struct SnArguments*);
CAPI VALUE snow_get_member(SN_P, VALUE self, SnSymbol member);
CAPI VALUE snow_set_member(SN_P, VALUE self, SnSymbol member, VALUE val);
CAPI void snow_modify(SN_P, struct SnObject* object);
CAPI const char* snow_value_to_cstr(SN_P, VALUE val);

CAPI void snow_printf(SN_P, const char* fmt, size_t num_args, ...);
CAPI void snow_vprintf(SN_P, const char* fmt, size_t num_args, va_list ap);

#endif /* end of include guard: SNOW_H_ILWGKE1Z */
