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
struct SnFunction;
struct SnString;

CAPI SN_P snow_init(struct SnVM*);
CAPI const char* snow_version();
CAPI SN_P snow_get_process();
CAPI SnType snow_get_type(VALUE v);

CAPI struct SnObject* snow_get_global_object();
CAPI struct SnObject** _snow_get_global_object_ptr(); // for GC
CAPI VALUE snow_set_global(SnSymbol sym, VALUE val);
CAPI VALUE snow_get_global(SnSymbol sym);

CAPI struct SnFunction* snow_compile(const char* module_name, const char* source);

CAPI VALUE snow_call(VALUE function, VALUE self, size_t num_args, const VALUE* args);
#define SN_CALL_METHOD(SELF, NAME, NUM_ARGS, ARGS) snow_call(snow_get_method(SELF, snow_sym(NAME)), SELF, NUM_ARGS, ARGS)
CAPI VALUE snow_call_with_named_arguments(VALUE function, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args);
CAPI struct SnFunction* snow_get_method(VALUE self, SnSymbol member);

CAPI VALUE snow_get_member(VALUE self, SnSymbol member);
CAPI VALUE snow_set_member(VALUE self, SnSymbol member, VALUE val);

CAPI struct SnString* snow_value_to_string(VALUE val);
CAPI const char* snow_value_to_cstr(VALUE val);
CAPI void snow_printf(const char* fmt, size_t num_args, ...);
CAPI void snow_vprintf(const char* fmt, size_t num_args, va_list ap);

#endif /* end of include guard: SNOW_H_ILWGKE1Z */
