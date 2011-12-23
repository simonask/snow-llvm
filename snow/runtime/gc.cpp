#include "snow/gc.hpp"

#include "snow/object.hpp"
#include "snow/fiber.hpp"
#include "fiber-internal.hpp"

#include "allocator.hpp"
#include "linkheap.hpp"
#include "snow/util.hpp"

#include <stdlib.h>
#include <vector>
#include <pthread.h>

namespace snow {
	namespace gc_internal {
		static Allocator allocator;
		static LinkHeap<Value> external_roots;
		static size_t num_objects = 0;

		Object* allocate_object(const Type* type) {
			ASSERT(sizeof(Object) <= SN_CACHE_LINE_SIZE - sizeof(void*));
			Object* obj = allocator.allocate();
			obj->type = type;
			void* data = obj + 1;
			if (type) {
				if (type->data_size + sizeof(Object) > SN_CACHE_LINE_SIZE) {
					void* heap_data = new byte[type->data_size];
					*(void**)data = heap_data;
					data = heap_data;
				}
				type->initialize(data);
			}
			++num_objects;
			return obj;
		}

		void free_object(Object* obj) {
			const Type* type = obj->type;
			if (type) {
				type->finalize(object_get_private(obj, type));
				if (type->data_size + sizeof(Object) > SN_CACHE_LINE_SIZE) {
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
	}
	
	void gc() {
	}
	
	Value* gc_create_root(Value initial_value) {
		Value* root = gc_internal::external_roots.alloc();
		*root = initial_value;
		return root;
	}
	
	Value gc_free_root(Value* root) {
		Value v = *root;
		*root = NULL;
		gc_internal::external_roots.free(root);
		return v;
	}
	
	Object* gc_allocate_object(const Type* type) {
		Object* obj = gc_internal::allocate_object(type);
		ASSERT(((intptr_t)obj & 0xf) == 0); // unaligned object allocation!
		return obj;
	}
}
