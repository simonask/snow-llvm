#include "snow/gc.h"
#include "snow/object.h"
#include "snow/type.h"
#include "snow/process.h"
#include "snow/map.h"
#include "snow/array.h"
#include "snow/function.h"
#include "snow/str.h"

#include "linkheap.hpp"

#include <stdlib.h>
#include <vector>

CAPI SnMap** _snow_get_global_storage(); // internal symbol

namespace {
	using namespace snow;
	
	static const size_t GC_OBJECT_SIZE = SN_OBJECT_MAX_SIZE;
	
	static int64_t gc_collection_threshold = 1000;
	static int64_t gc_num_objects = 0;
	static int64_t gc_max_num_objects = 0;
	static int64_t gc_reachable_objects = 0;
	
	struct GCObject {
		byte data[GC_OBJECT_SIZE];
	};
	
	enum GCFlags {
		GC_UNREACHABLE = 0,
		GC_REACHABLE = 1,
		GC_DELETED = 2,
	};
	
	static LinkHeap<GCObject> gc_heap;
	static uint8_t* gc_flags = NULL;
	
	static const void** stack_top = NULL;
	static const void** stack_bottom = NULL;
	static intptr_t heap_min = NULL;
	static intptr_t heap_max = NULL;
	
	void gc_mark_value(VALUE);
	
	void gc_mark_object(SnObjectBase* x) {
		switch (x->type) {
			case SnObjectType: {
				SnObject* object = (SnObject*)x;
				gc_mark_value(object->prototype);
				gc_mark_value(object->members);
				gc_mark_value(object->properties);
				gc_mark_value(object->included_modules);
				break;
			}
			case SnStringType: /* no roots */ break;
			case SnArrayType: {
				SnArray* array = (SnArray*)x;
				for (uint32_t i = 0; i < array->size; ++i) {
					gc_mark_value(array->data[i]);
				}
				break;
			}
			case SnMapType: {
				snow_map_for_each_gc_root((SnMap*)x, gc_mark_value);
				break;
			}
			case SnFunctionType: {
				SnFunction* function = (SnFunction*)x;
				gc_mark_value(function->definition_context);
				break;
			}
			case SnFunctionCallContextType: {
				SnFunctionCallContext* context = (SnFunctionCallContext*)x;
				gc_mark_value(context->function);
				gc_mark_value(context->caller);
				gc_mark_value(context->self);
				for (size_t i = 0; i < context->function->descriptor->num_locals; ++i) {
					gc_mark_value(context->locals[i]);
				}
				gc_mark_value(context->arguments);
				break;
			}
			case SnArgumentsType: {
				SnArguments* args = (SnArguments*)args;
				for (size_t i = 0; i < args->size; ++i) {
					gc_mark_value(args->data[i]);
				}
				break;
			}
			case SnPointerType:
			default: {
				fprintf(stderr, "GC: ERROR! Unknown object %p, type %x, cannot mark!\n", x, x->type);
				TRAP();
			}
		}
	}
	
	void gc_mark_value(VALUE obj) {
		ASSERT(gc_flags); // not in GC cycle
		if (snow_is_object(obj)) {
			int64_t idx = gc_heap.index_of(obj);
			ASSERT(idx < gc_max_num_objects);
			if (idx >= 0) {
				if (gc_flags[idx] == GC_UNREACHABLE) {
					// not previously marked, so let's mark it, and traverse its referenced objects
					gc_flags[idx] = GC_REACHABLE;
					++gc_reachable_objects;
					
					SnObjectBase* object = (SnObjectBase*)obj;
					gc_mark_object(object);
				}
			}
		}
	}
	
	void gc_mark_globals() {
		gc_mark_value(*_snow_get_global_storage());
	}
	
	bool looks_like_gc_root(VALUE _x) {
		intptr_t x = (intptr_t)_x;
		return (x & 0xf) == 0 && x >= heap_min && x < heap_max;
	}
	
	void gc_mark_stack() {
		fprintf(stderr, "GC: marking stack from %p to %p\n", stack_bottom, stack_top);
		for (const void** p = stack_bottom; p < stack_top; ++p) {
			if (looks_like_gc_root((VALUE)*p)) {
				gc_mark_value((VALUE)*p);
			}
		}
	}
	
	void gc_free_object(SnObjectBase* obj) {
		fprintf(stderr, "GC: Freeing object %p\n", obj);
		switch (obj->type) {
			case SnObjectType: break;
			case SnStringType: snow_finalize_string((SnString*)obj); break;
			case SnArrayType: snow_finalize_array((SnArray*)obj); break;
			case SnMapType: snow_finalize_map((SnMap*)obj); break;
			case SnFunctionType: snow_finalize_function((SnFunction*)obj); break;
			case SnFunctionCallContextType: snow_finalize_function_call_context((SnFunctionCallContext*)obj); break;
			case SnPointerType:
			default: {
				fprintf(stderr, "GC: Cannot finalize unknown object type!\n");
				TRAP();
			}
		}
		
		gc_heap.free((GCObject*)obj);
	}
}

CAPI {
	void snow_init_gc(const void** stk_top) {
		stack_top = stk_top;
	}
	
	void snow_gc() {
		ASSERT(!stack_bottom); // GC is already in progress!
		ASSERT(stack_top); // GC is not initialized!
		
		// initialize everything
		gc_heap.get_min_max(heap_min, heap_max);
		if (heap_max < heap_min) return; // nothing is allocated
		const void* stk;
		stack_bottom = &stk;
		gc_max_num_objects = gc_heap.max_num_objects();
		gc_flags = new uint8_t[gc_max_num_objects];
		memset(gc_flags, GC_UNREACHABLE, gc_max_num_objects);
		gc_reachable_objects = 0;
		
		// mark already deleted objects specially, to avoid double-free
		const void* f = gc_heap.get_first_free();
		while (f) {
			int64_t idx = gc_heap.index_of(f);
			ASSERT(idx >= 0 && idx < gc_num_objects);
			gc_flags[idx] = GC_DELETED;
			f = gc_heap.get_next_free(f);
		}
		
		// mark everything
		gc_mark_globals();
		gc_mark_stack();
		
		// free unreachable objects
		int64_t freed_objects = 0;
		for (int64_t i = 0; i < gc_max_num_objects; ++i) {
			if (gc_flags[i] == GC_UNREACHABLE) {
				GCObject* obj = gc_heap.object_at_index(i);
				if (obj) {
					gc_free_object((SnObjectBase*)obj);
					++freed_objects;
				}
			}
		}
		
		fprintf(stderr, "GC: Statistics: freed=%lld, reachable=%lld, max=%lld, threshold=%lld\n", freed_objects, gc_reachable_objects, gc_max_num_objects, gc_collection_threshold);
		
		// adjust heuristics
		if (freed_objects < gc_num_objects/2) {
			gc_collection_threshold += gc_collection_threshold/2;
		}
		gc_num_objects -= freed_objects;
		
		delete gc_flags;
		gc_flags = NULL;
		stack_bottom = NULL;
	}
	
	SnObjectBase* snow_gc_alloc_object(size_t sz, SnType type) {
		ASSERT(sz <= GC_OBJECT_SIZE);
		
		// Disabled GC for now -- reenable when we have a real debugger.
		//if (gc_num_objects >= gc_collection_threshold) {
		//	snow_gc();
		//}
		
		SnObjectBase* obj = (SnObjectBase*)gc_heap.alloc();
		ASSERT(((intptr_t)obj & 0xf) == 0); // unaligned object allocation!
		obj->type = type;
		++gc_num_objects;
		return obj;
	}
}
