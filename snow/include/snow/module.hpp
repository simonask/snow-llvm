#pragma once
#ifndef MODULE_H_BW7OGHRV
#define MODULE_H_BW7OGHRV

#include "snow/basic.h"
#include "snow/objectptr.hpp"
#include "snow/str.hpp"

struct SnObject;

namespace snow {
	struct Array;
	
	typedef Value(*ModuleInitFunc)();
	typedef int(*TestSuiteFunc)();
	
	ObjectPtr<Array> get_load_paths();
	Value get_global_module();
	
	Value import(StringConstPtr str);
	Value load(StringConstPtr str);
	Value load_in_global_module(StringConstPtr path);
	Value load_module_from_source(StringConstPtr source);
	Value load_module(StringConstPtr path, const char* entry_point);
	
	Value eval_in_global_module(StringConstPtr source);
	
	void _module_finished(Value module);
}

#endif /* end of include guard: MODULE_H_BW7OGHRV */
