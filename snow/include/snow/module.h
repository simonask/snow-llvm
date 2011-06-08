#pragma once
#ifndef MODULE_H_BW7OGHRV
#define MODULE_H_BW7OGHRV

#include "snow/basic.h"

struct SnObject;

CAPI struct SnObject* snow_get_load_paths();
CAPI struct SnObject* snow_get_global_module();

CAPI struct SnObject* snow_import(SnObject* file_str);
CAPI struct SnObject* snow_load(SnObject* file_str);
CAPI VALUE snow_load_in_global_module(SnObject* path_str);
CAPI struct SnObject* snow_load_module_from_source(SnObject* source_str);
CAPI struct SnObject* snow_load_module(SnObject* path_str, const char* entry_point);

CAPI VALUE snow_eval_in_global_module(SnObject* source_str);

CAPI void _snow_module_finished(struct SnObject* module);

#endif /* end of include guard: MODULE_H_BW7OGHRV */
