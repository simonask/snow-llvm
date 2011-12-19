#include "snow/gc.hpp"

#include "snow/object.hpp"
#include "snow/fiber.hpp"
#include "fiber-internal.hpp"

#include "allocator.hpp"
#include "lock.h"
#include "linkheap.hpp"
#include "snow/util.hpp"

#include <stdlib.h>
#include <vector>
#include <pthread.h>

namespace snow {
	namespace gc_internal {
		static int64_t collection_threshold = 1000;
		static int64_t num_objects = 0;
		static int64_t max_num_objects = 0;
		static int64_t reachable_objects = 0;
		static uint8_t* mark_bits = NULL;

		static Allocator allocator;
		static LinkHeap<SnObject*> external_roots;

		// TODO: Per-thread stack tops.
		static const byte* system_stack_top = NULL;

		static void mark_object(SnObject* object);
		static void mark_value(VALUE);
		static void mark_value_range(const VALUE*, const VALUE*);
		static void mark_fiber_stack(const byte* bottom, const byte* top);
		static void mark_system_stack(const byte* bottom);
		static void free_object(SnObject*);

		void mark_value(VALUE value) {
			if (is_object(value)) {
				SnObject* object = (SnObject*)value;
			}
		}

		void mark_value_range(const VALUE* start, const VALUE* end) {
			const VALUE* p = start;
			while (p < end) {
				mark_value(*p++);
			}
		}

		void mark_object(SnObject* x) {
			//fprintf(stderr, "GC: Marking object %p of type 0x%x\n", x, x->type);	
		}

		void mark_external_roots() {
			size_t num_external_roots = external_roots.get_number_of_allocated_upper_bound();
			for (size_t i = 0; i < num_external_roots; ++i) {
				SnObject** root = external_roots.object_at_index(i);
				mark_value(*root);
			}
		}

		void mark_fiber_stack(const byte* _bottom, const byte* _top) {
			//fprintf(stderr, "GC: Marking fiber stack: %p - %p\n", _bottom, _top);
			ASSERT(_bottom < _top);
			intptr_t bottom = (intptr_t)_bottom;
			bottom = bottom + (sizeof(VALUE) - bottom % sizeof(VALUE));
			VALUE* p = (VALUE*)bottom;
			VALUE* top = (VALUE*)_top;
			while (p < top) {
				mark_value(*p++);
			}
		}

		void mark_system_stack(const byte* _bottom) {
			ASSERT(_bottom < gc_internal::system_stack_top);
			//fprintf(stderr, "GC: Marking system stack: %p - %p\n", _bottom, gc_system_stack_top);
			mark_fiber_stack(_bottom, system_stack_top);
		}

		void delete_unmarked_objects() {
		}

		SnObject* allocate_object(const Type* type) {
			ASSERT(sizeof(SnObject) <= SN_CACHE_LINE_SIZE - sizeof(void*));
			SnObject* obj = allocator.allocate();
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
			++num_objects;
			return obj;
		}

		void free_object(SnObject* obj) {
			const Type* type = obj->type;
			if (type) {
				type->finalize(object_get_private(obj, type));
				if (type->data_size + sizeof(SnObject) > SN_CACHE_LINE_SIZE) {
					void* heap_data = *(void**)(obj + 1);
					type->finalize(heap_data);
					delete[] reinterpret_cast<byte*>(heap_data);
				} else {
					type->finalize(obj + 1);
				}
				snow::dealloc_range(obj->members, obj->num_alloc_members);
			}
			allocator.free(obj);
			--num_objects;
		}
	}
	
	void init_gc(const void** stk_top) {
		gc_internal::system_stack_top = (byte*)stk_top;
	}
	
	void gc() {
		SnObject* fiber = get_current_fiber();
		fprintf(stderr, "GC: Starting collection from fiber %p (threshold: %llu)\n", fiber, gc_internal::collection_threshold);
		fiber_suspend_for_garbage_collection(fiber);
		size_t num_objects = gc_internal::allocator.get_max_num_objects();
		size_t num_bytes = num_objects / 8 + 1;
		gc_internal::mark_bits = new uint8_t[num_bytes];
		memset(gc_internal::mark_bits, 0, num_bytes);
	
		// TODO: This must be done asynchronously in a thread, to avoid running out of
		// fiber stack space during collection.
		
		// Current fiber is stored in an external root, and the stack is marked
		// when a fiber is marked.
		gc_internal::mark_external_roots();
		
		ssize_t num_objects_before = gc_internal::num_objects;
		gc_internal::delete_unmarked_objects();

		// This should make sure that the threshold after a collection grows with the
		// demand for objects.
		ssize_t new_threshold = 2 * gc_internal::num_objects;
		if (new_threshold > gc_internal::collection_threshold) {
			gc_internal::collection_threshold = new_threshold;
			fprintf(stderr, "GC: New threshold: %llu\n", gc_internal::collection_threshold);
		}
		
		delete[] gc_internal::mark_bits;
		gc_internal::mark_bits = NULL;
		fprintf(stderr, "GC: Collection done. Freed %llu objects.\n", num_objects_before - gc_internal::num_objects);
	}
	
	SnObject** gc_create_root(SnObject* initial_value) {
		SnObject** root = gc_internal::external_roots.alloc();
		*root = initial_value;
		return root;
	}
	
	SnObject* gc_free_root(SnObject** root) {
		SnObject* v = *root;
		*root = NULL;
		gc_internal::external_roots.free(root);
		return v;
	}
	
	SnObject* gc_allocate_object(const struct Type* type) {
		if (gc_internal::num_objects >= gc_internal::collection_threshold) {
			//gc();
		}
		
		SnObject* obj = gc_internal::allocate_object(type);
		ASSERT(((intptr_t)obj & 0xf) == 0); // unaligned object allocation!
		return obj;
	}
	
	void gc_rdlock(const SnObject* object, void* gc_root) {
		Allocator::Block* block = gc_internal::allocator.get_block_for_object_fast(object);
		SN_RWLOCK_RDLOCK(&block->lock);
		// TODO: Update gc_root if object has moved, and acquire new lock instead
	}
	
	void gc_wrlock(const SnObject* object, void* gc_root) {
		Allocator::Block* block = gc_internal::allocator.get_block_for_object_fast(object);
		SN_RWLOCK_WRLOCK(&block->lock);
		// TODO: Update gc_root if object has moved, and acquite new lock instead
	}
	
	void gc_unlock(const SnObject* object) {
		Allocator::Block* block = gc_internal::allocator.get_block_for_object_fast(object);
		SN_RWLOCK_UNLOCK(&block->lock);
	}
}
