#pragma once
#ifndef MODULE_H_BW7OGHRV
#define MODULE_H_BW7OGHRV

#include "snow/basic.h"

struct SnObject;
struct SnArray;

CAPI struct SnArray* snow_get_load_paths();
CAPI struct SnObject* snow_get_global_module();

CAPI struct SnObject* snow_import(const char* file);
CAPI struct SnObject* snow_load(const char* file);
CAPI VALUE snow_load_in_global_module(const char* file);
CAPI struct SnObject* snow_load_module_from_source(const char* source);
CAPI struct SnObject* snow_load_precompiled_module(const char* file);
CAPI struct SnObject* snow_load_native_module(const char* file, const char* entry_point);

CAPI VALUE snow_eval_in_global_module(const char* source);

CAPI void _snow_module_finished(struct SnObject* module);

#endif /* end of include guard: MODULE_H_BW7OGHRV */
