#include "snow/gc.h"

#include "snow/array.h"
#include "snow/class.h"
#include "snow/fiber.h"
#include "snow/function.h"
#include "snow/map.h"
#include "snow/object.h"
#include "snow/pointer.h"
#include "snow/str.h"
#include "snow/type.h"

#include "allocator.hpp"
#include "lock.h"
#include "linkheap.hpp"

#include <stdlib.h>
#include <vector>
#include <pthread.h>

CAPI void snow_finalize_object(SnObject*);
CAPI void snow_finalize_class(SnClass*);
CAPI void snow_finalize_string(SnString*);
CAPI void snow_finalize_array(SnArray*);
CAPI void snow_finalize_map(SnMap*);
CAPI void snow_finalize_function(SnFunction*);
CAPI void snow_finalize_call_frame(SnCallFrame*);
CAPI void snow_finalize_arguments(SnArguments*);
CAPI void snow_finalize_pointer(SnPointer*);
CAPI void snow_finalize_fiber(SnFiber*);

namespace {
	using namespace snow;
	
	static int64_t gc_collection_threshold = 1000;
	static int64_t gc_num_objects = 0;
	static int64_t gc_max_num_objects = 0;
	static int64_t gc_reachable_objects = 0;
	static uint8_t* gc_mark_bits = NULL;
	
	struct GCObject {
		byte data[SN_OBJECT_SIZE];
	};
	
	enum GCFlags {
		GC_UNREACHABLE = 0,
		GC_REACHABLE = 1,
		GC_DELETED = 2,
	};
	
	static Allocator gc_allocator;
	static LinkHeap<VALUE> gc_external_roots;
	
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
			size_t block_index;
			size_t object_index;
			Allocator::Block* block = gc_allocator.get_block_for_object_safe(object, &block_index, &object_index);
			if (block) {
				// value is a valid root
				// TODO: Consider locking, when we implement multithreading.
				size_t mark_bit_index = block_index * Allocator::OBJECTS_PER_BLOCK + object_index;
				size_t mark_bit_byte = mark_bit_index / 8;
				size_t mark_bit_bit = mark_bit_index % 8;
				size_t mark_bit_mask = 1UL << mark_bit_bit;
				bool was_marked = (gc_mark_bits[mark_bit_byte] & mark_bit_mask) == mark_bit_mask;
				if (!was_marked) {
					gc_mark_bits[mark_bit_byte] |= mark_bit_mask;
					gc_mark_object(object);
				}
			}
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
		gc_mark_value(x->cls);
		for (size_t i = 0; i < x->num_members; ++i) {
			gc_mark_value(x->members[i]);
		}
		
		switch (x->type) {
			case SnObjectType: break;
			case SnStringType: break;
			case SnClassType: {
				SnClass* cls = (SnClass*)x;
				for (size_t i = 0; i < cls->num_methods; ++i) {
					switch (cls->methods[i].type) {
						case SnMethodTypeFunction: {
							gc_mark_value(cls->methods[i].function);
							break;
						}
						case SnMethodTypeProperty: {
							gc_mark_value(cls->methods[i].property.getter);
							gc_mark_value(cls->methods[i].property.setter);
							break;
						}
						default: break;
					}
				}
				gc_mark_object((SnObject*)cls->super);
				break;
			}
			case SnArrayType: {
				SnArray* array = (SnArray*)x;
				gc_mark_value_range(array->data, array->data + array->size);
				break;
			}
			case SnMapType: {
				SnMap* map = (SnMap*)x;
				snow_map_for_each_gc_root(map, gc_mark_value);
				break;
			}
			case SnFunctionType: {
				SnFunction* function = (SnFunction*)x;
				gc_mark_value(function->definition_context);
				const size_t num_variable_references = function->descriptor->num_variable_references;
				for (size_t i = 0; i < num_variable_references; ++i) {
					gc_mark_value(*function->variable_references[i]);
				}
				break;
			}
			case SnCallFrameType: {
				SnCallFrame* frame = (SnCallFrame*)x;
				gc_mark_value(frame->function);
				gc_mark_value(frame->caller);
				gc_mark_value(frame->self);
				gc_mark_value_range(frame->locals, frame->locals + frame->function->descriptor->num_locals);
				gc_mark_value(frame->arguments);
				gc_mark_value(frame->module);
				break;
			}
			case SnArgumentsType: {
				SnArguments* args = (SnArguments*)x;
				gc_mark_value_range(args->data, args->data + args->size);
				break;
			}
			case SnPointerType: {
				// TODO: Provide a callback for GC operations in pointers.
				break;
			}
			case SnFiberType: {
				SnFiber* fiber = (SnFiber*)x;
				gc_mark_value(fiber->current_frame);
				
				if (fiber->stack) {
					// non-system fiber
					gc_mark_fiber_stack(fiber->stack, fiber->suspended_stack_boundary);
				} else {
					gc_mark_system_stack(fiber->suspended_stack_boundary);
				}
				gc_mark_value(fiber->link);
				break;
			}
			default: {
				fprintf(stderr, "ERROR! GC encountered an unknown object type. Memory corruption likely. (object = %p, object type = 0x%x)\n", x, x->type);
				TRAP();
			}
		}
	}
	
	void gc_mark_external_roots() {
		size_t num_external_roots = gc_external_roots.get_number_of_allocated_upper_bound();
		for (size_t i = 0; i < num_external_roots; ++i) {
			VALUE* root = gc_external_roots.object_at_index(i);
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
		intptr_t bottom = (intptr_t)_bottom;
		bottom = bottom + (sizeof(VALUE) - bottom % sizeof(VALUE));
		VALUE* p = (VALUE*)bottom;
		VALUE* top = (VALUE*)gc_system_stack_top;
		while (p < top) {
			gc_mark_value(*p++);
		}
	}
	
	void gc_delete_unmarked_objects() {
		size_t num_blocks = gc_allocator.get_num_blocks();
		for (size_t i = 0; i < num_blocks; ++i) {
			Allocator::Block* block = gc_allocator.get_block(i);
			for (size_t j = 0; j < block->num_allocated_upper_bound(); ++j) {
				size_t object_index = i * Allocator::OBJECTS_PER_BLOCK + j;
				SnObject* object = (SnObject*)(block->begin + j);
				if (block->is_allocated(object)) {
					size_t mark_byte = object_index / 8;
					size_t mark_bit = object_index % 8;
					size_t mark_mask = 1 << mark_bit;
					bool is_marked = (gc_mark_bits[mark_byte] & mark_mask) == mark_mask;
					if (!is_marked) {
						gc_free_object(object);
					}
				}
			}
		}
	}
	
	void gc_free_object(SnObject* obj) {
		//fprintf(stderr, "GC: Freeing object %p (type: 0x%x)\n", obj, obj->type);
		switch (obj->type) {
			case SnObjectType:    break;
			case SnClassType:     snow_finalize_class((SnClass*)obj);          break;
			case SnStringType:    snow_finalize_string((SnString*)obj);        break;
			case SnArrayType:     snow_finalize_array((SnArray*)obj);          break;
			case SnMapType:       snow_finalize_map((SnMap*)obj);              break;
			case SnFunctionType:  snow_finalize_function((SnFunction*)obj);    break;
			case SnCallFrameType: snow_finalize_call_frame((SnCallFrame*)obj); break;
			case SnArgumentsType: snow_finalize_arguments((SnArguments*)obj);  break;
			case SnPointerType:   snow_finalize_pointer((SnPointer*)obj);      break;
			case SnFiberType:     snow_finalize_fiber((SnFiber*)obj);          break;
			default: {
				fprintf(stderr, "GC: Cannot finalize unknown object type!\n");
				TRAP();
			}
		}
		snow_finalize_object(obj);
		
		gc_allocator.free(obj);
		--gc_num_objects;
	}
}

CAPI {
	void snow_init_gc(const void** stk_top) {
		gc_system_stack_top = (byte*)stk_top;
	}
	
	void snow_gc() {
		SnFiber* fiber = snow_get_current_fiber();
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
	
	VALUE* snow_gc_create_root(VALUE initial_value) {
		VALUE* root = gc_external_roots.alloc();
		*root = initial_value;
		return root;
	}
	
	VALUE snow_gc_free_root(VALUE* root) {
		VALUE v = *root;
		*root = NULL;
		gc_external_roots.free(root);
		return v;
	}
	
	SnObject* snow_gc_alloc_object(SnType type) {
		size_t sz = snow_size_of_type(type);
		ASSERT(sz <= SN_OBJECT_SIZE);
		ASSERT(!snow_is_immediate_type(type) && type != SnAnyType);
		
		if (gc_num_objects >= gc_collection_threshold) {
			//snow_gc();
		}
		
		SnObject* obj = gc_allocator.allocate();
		ASSERT(((intptr_t)obj & 0xf) == 0); // unaligned object allocation!
		obj->type = type;
		++gc_num_objects;
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
