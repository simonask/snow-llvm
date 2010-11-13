#include "snow/gc.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"

#include <stdlib.h>

void snow_gc() {
	TRAP();
}

SnObjectBase* snow_gc_alloc_object(size_t sz, SnType type) {
	// XXX: LEAKING STUB!!
	sz = sz > SN_OBJECT_MAX_SIZE ? sz : SN_OBJECT_MAX_SIZE;
	SnObjectBase* obj = (SnObjectBase*)malloc(sz);
	obj->type = type;
	return obj;
}