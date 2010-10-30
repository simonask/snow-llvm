#include "snow/gc.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"

#include <stdlib.h>

void snow_gc() {
	TRAP();
}

SnObjectBase* snow_gc_alloc_object(SnType type) {
	// XXX: LEAKING STUB!!
	SnObjectBase* obj = (SnObjectBase*)malloc(SN_CACHE_LINE_SIZE);
	obj->type = type;
	return obj;
}