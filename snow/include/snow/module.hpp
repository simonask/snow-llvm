#pragma once
#ifndef MODULE_H_BW7OGHRV
#define MODULE_H_BW7OGHRV

#include "snow/basic.h"
#include "snow/objectptr.hpp"
#include "snow/str.hpp"

struct SnObject;

namespace snow {
	struct Array;
	
	typedef VALUE(*ModuleInitFunc)();
	typedef int(*TestSuiteFunc)();
	
	ObjectPtr<Array> get_load_paths();
	SnObject* get_global_module();
	
	SnObject* import(StringConstPtr str);
	SnObject* load(StringConstPtr str);
	VALUE load_in_global_module(StringConstPtr path);
	SnObject* load_module_from_source(StringConstPtr source);
	SnObject* load_module(StringConstPtr path, const char* entry_point);
	
	VALUE eval_in_global_module(StringConstPtr source);
	
	void _module_finished(SnObject* module);
}

#endif /* end of include guard: MODULE_H_BW7OGHRV */
