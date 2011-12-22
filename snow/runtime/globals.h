#pragma once
#ifndef GLOBALS_H_M174V2MS
#define GLOBALS_H_M174V2MS

#include "snow/basic.h"
#include "snow/value.hpp"

struct SnObject;

void snow_init_globals();
snow::Value snow_get_vm_interface();

#endif /* end of include guard: GLOBALS_H_M174V2MS */
