#pragma once
#ifndef VM_INTERN_H_NAVDIA6L
#define VM_INTERN_H_NAVDIA6L

#include "snow/basic.h"


namespace snow {
	struct StackFrameMap {
		int32_t num_roots;
	};
	
	struct StackFrameEntry {
		StackFrameEntry* previous; // caller's
		StackFrameMap* map;
		void* roots[];
	};
}




#endif /* end of include guard: VM_INTERN_H_NAVDIA6L */
