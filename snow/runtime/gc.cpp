#include "snow/gc.hpp"

#include "snow/object.hpp"
#include "snow/fiber.hpp"
#include "fiber-internal.h"

#include "allocator.hpp"
#include "lock.h"
#include "linkheap.hpp"
#include "util.hpp"

#include <stdlib.h>
#include <vector>
#include <pthread.h>

namespace {
	using namespace snow;
	
	static int64_t gc_collection_threshold = 1000;
	static int64_t gc_num_objects = 0;
	static int64_t gc_max_num_objects = 0;
	static int64_t gc_reachable_objects = 0;
	static uint8_t* gc_mark_bits = NULL;
	
	enum GCFlags {
		GC_UNREACHABLE = 0,
		GC_REACHABLE = 1,
		GC_DELETED = 2,
	};
	
	static Allocator gc_allocator;
	static LinkHeap<SnObject*> gc_external_roots;
	
	// TODO: Per-thread stack tops.
	static const byte* gc_system_stack_top = NULL;
	
	static void gc_mark_object(SnObject* object);
	static void gc_mark_value(VALUE);
	static void gc_mark_value_range(const VALUE*, const VALUE*);
	static void gc_mark_fiber_stack(const byte* bottom, const byte* top);
	static void gc_mark_system_stack(const byte* bottom);
	static void gc_free_object(SnObject*);
	
	void gc_mark_value(VALUE value) {
		if (snow_is_object(value)) {
			SnObject* object = (SnObject*)value;
		}
	}
	
	void gc_mark_value_range(const VALUE* start, const VALUE* end) {
		const VALUE* p = start;
		while (p < end) {
			gc_mark_value(*p++);
		}
	}
	
	void gc_mark_object(SnObject* x) {
		//fprintf(stderr, "GC: Marking object %p of type 0x%x\n", x, x->type);	
	}
	
	void gc_mark_external_roots() {
		size_t num_external_roots = gc_external_roots.get_number_of_allocated_upper_bound();
		for (size_t i = 0; i < num_external_roots; ++i) {
			SnObject** root = gc_external_roots.object_at_index(i);
			gc_mark_value(*root);
		}
	}
	
	void gc_mark_fiber_stack(const byte* _bottom, const byte* _top) {
		//fprintf(stderr, "GC: Marking fiber stack: %p - %p\n", _bottom, _top);
		ASSERT(_bottom < _top);
		intptr_t bottom = (intptr_t)_bottom;
		bottom = bottom + (sizeof(VALUE) - bottom % sizeof(VALUE));
		VALUE* p = (VALUE*)bottom;
		VALUE* top = (VALUE*)_top;
		while (p < top) {
			gc_mark_value(*p++);
		}
	}
	
	void gc_mark_system_stack(const byte* _bottom) {
		ASSERT(_bottom < gc_system_stack_top);
		//fprintf(stderr, "GC: Marking system stack: %p - %p\n", _bottom, gc_system_stack_top);
		gc_mark_fiber_stack(_bottom, gc_system_stack_top);
	}
	
	void gc_delete_unmarked_objects() {
	}
	
	SnObject* gc_allocate_object(const Type* type) {
		ASSERT(sizeof(SnObject) <= SN_CACHE_LINE_SIZE - sizeof(void*));
		SnObject* obj = gc_allocator.allocate();
		obj->type = type;
		void* data = obj + 1;
		if (type) {
			if (type->data_size + sizeof(SnObject) > SN_CACHE_LINE_SIZE) {
				void* heap_data = new byte[type->data_size];
				*(void**)data = heap_data;
				data = heap_data;
			}
			type->initialize(data);
		}
		++gc_num_objects;
		return obj;
	}
	
	void gc_free_object(SnObject* obj) {
		const Type* type = obj->type;
		if (type) {
			type->finalize(snow_object_get_private(obj, type));
			if (type->data_size + sizeof(SnObject) > SN_CACHE_LINE_SIZE) {
				void* heap_data = *(void**)(obj + 1);
				type->finalize(heap_data);
				delete[] reinterpret_cast<byte*>(heap_data);
			} else {
				type->finalize(obj + 1);
			}
			snow::dealloc_range(obj->members, obj->num_alloc_members);
		}
		gc_allocator.free(obj);
		--gc_num_objects;
	}
}

CAPI {
	void snow_init_gc(const void** stk_top) {
		gc_system_stack_top = (byte*)stk_top;
	}
	
	void snow_gc() {
		SnObject* fiber = snow_get_current_fiber();
		fprintf(stderr, "GC: Starting collection from fiber %p (threshold: %llu)\n", fiber, gc_collection_threshold);
		snow_fiber_suspend_for_garbage_collection(fiber);
		size_t num_objects = gc_allocator.get_max_num_objects();
		size_t num_bytes = num_objects / 8 + 1;
		gc_mark_bits = new uint8_t[num_bytes];
		memset(gc_mark_bits, 0, num_bytes);
	
		// TODO: This must be done asynchronously in a thread, to avoid running out of
		// fiber stack space during collection.
		
		// Current fiber is stored in an external root, and the stack is marked
		// when a fiber is marked.
		gc_mark_external_roots();
		
		ssize_t num_objects_before = gc_num_objects;
		gc_delete_unmarked_objects();

		// This should make sure that the threshold after a collection grows with the
		// demand for objects.
		ssize_t new_threshold = 2 * gc_num_objects;
		if (new_threshold > gc_collection_threshold) {
			gc_collection_threshold = new_threshold;
			fprintf(stderr, "GC: New threshold: %llu\n", gc_collection_threshold);
		}
		
		delete[] gc_mark_bits;
		gc_mark_bits = NULL;
		fprintf(stderr, "GC: Collection done. Freed %llu objects.\n", num_objects_before - gc_num_objects);
	}
	
	SnObject** snow_gc_create_root(SnObject* initial_value) {
		SnObject** root = gc_external_roots.alloc();
		*root = initial_value;
		return root;
	}
	
	SnObject* snow_gc_free_root(SnObject** root) {
		SnObject* v = *root;
		*root = NULL;
		gc_external_roots.free(root);
		return v;
	}
	
	SnObject* snow_gc_allocate_object(const struct Type* type) {
		if (gc_num_objects >= gc_collection_threshold) {
			//snow_gc();
		}
		
		SnObject* obj = gc_allocate_object(type);
		ASSERT(((intptr_t)obj & 0xf) == 0); // unaligned object allocation!
		return obj;
	}
	
	void snow_gc_rdlock(const SnObject* object, void* gc_root) {
		Allocator::Block* block = gc_allocator.get_block_for_object_fast(object);
		SN_RWLOCK_RDLOCK(&block->lock);
		// TODO: Update gc_root if object has moved, and acquire new lock instead
	}
	
	void snow_gc_wrlock(const SnObject* object, void* gc_root) {
		Allocator::Block* block = gc_allocator.get_block_for_object_fast(object);
		SN_RWLOCK_WRLOCK(&block->lock);
		// TODO: Update gc_root if object has moved, and acquite new lock instead
	}
	
	void snow_gc_unlock(const SnObject* object) {
		Allocator::Block* block = gc_allocator.get_block_for_object_fast(object);
		SN_RWLOCK_UNLOCK(&block->lock);
	}
}
