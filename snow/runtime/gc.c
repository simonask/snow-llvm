#include "snow/gc.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"

#include <stdlib.h>

void snow_gc() {
	TRAP();
}

SnObject* snow_gc_create_object(SnProcess* p, const SnType* type) {
	// XXX: LEAKING STUB!!
	byte* mem = (byte*)malloc(sizeof(SnObject) + type->size);
	SnObject* obj = (SnObject*)mem;
	obj->type = type;
	obj->owner = p;
	obj->data = mem + sizeof(SnObject);
	type->initialize(p, obj);
	return obj;
}