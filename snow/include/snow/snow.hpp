#pragma once
#ifndef SNOW_H_ILWGKE1Z
#define SNOW_H_ILWGKE1Z

#include "snow/basic.h"
#include "snow/value.hpp"
#include "snow/symbol.hpp"

struct SnArguments;
struct SnObject;
struct SnCallFrame;

CAPI void snow_init(const char* lib_path);
CAPI void snow_finish();
CAPI int snow_main(int argc, char* const* argv);

CAPI const char* snow_version();
CAPI SnValueType snow_get_type(VALUE v);

CAPI struct SnObject* snow_get_global_object();
CAPI struct SnObject** _snow_get_global_object_ptr(); // for GC
CAPI VALUE snow_set_global(SnSymbol sym, VALUE val);
CAPI VALUE snow_get_global(SnSymbol sym);
CAPI VALUE snow_local_missing(struct SnCallFrame* frame, SnSymbol name);

CAPI struct SnObject* snow_get_class(VALUE value);
CAPI VALUE snow_call(VALUE function, VALUE self, size_t num_args, VALUE* args);
CAPI VALUE snow_call_with_arguments(VALUE function, VALUE self, const struct SnArguments* args);
CAPI VALUE snow_call_method(VALUE self, SnSymbol method_name, size_t num_args, VALUE* args);
CAPI VALUE snow_call_with_named_arguments(VALUE function, VALUE self, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args);
CAPI VALUE snow_call_method_with_named_arguments(VALUE self, SnSymbol method_name, size_t num_names, SnSymbol* names, size_t num_args, VALUE* args);
#define SNOW_CALL_METHOD(SELF, NAME, NUM_ARGS, ARGS) snow_call_method(SELF, snow_sym(NAME), NUM_ARGS, ARGS)
CAPI VALUE snow_get_method(VALUE self, SnSymbol member);

CAPI struct SnObject* snow_get_nearest_object(VALUE val);
CAPI VALUE snow_value_freeze(VALUE it);

CAPI struct SnObject* create_class_for_prototype(SnSymbol name, struct SnObject* proto);

CAPI struct SnObject* snow_value_to_string(VALUE val);
CAPI struct SnObject* snow_value_inspect(VALUE val);
CAPI void snow_printf(const char* fmt, size_t num_args, ...);
CAPI void snow_vprintf(const char* fmt, size_t num_args, va_list ap);

#endif /* end of include guard: SNOW_H_ILWGKE1Z */
