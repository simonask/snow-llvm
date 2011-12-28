#include "snow/gc.hpp"

#include "snow/object.hpp"
#include "snow/fiber.hpp"
#include "fiber-internal.hpp"

#include "allocator.hpp"
#include "linkheap.hpp"
#include "snow/util.hpp"

#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <pthread.h>

namespace snow {
	namespace {
		enum GCFlags {
			GCNoFlags = 0,
			GCReachable = 1,
			GCFreed = 2,
		};
		
		static struct {
			size_t num_objects;
			size_t collection_threshold;
			void** stack_top;
			void** stack_bottom;
			uint8_t* object_flags;
			size_t num_object_flags;
		} GC;
		
		static Allocator allocator;
		static std::vector<Value*> external_roots;
		
		void start_collection() {
			GC.num_object_flags = allocator.capacity();
			GC.object_flags = new uint8_t[GC.num_object_flags];
			snow::assign_range(GC.object_flags, (uint8_t)GCNoFlags, GC.num_object_flags);
		}
		
		void finish_collection() {
			GC.num_object_flags = 0;
			delete[] GC.object_flags;
			GC.object_flags = NULL;
		}
		
		void adjust_collection_threshold() {
			size_t candidate = GC.num_objects * 2;
			if (candidate > GC.collection_threshold) {
				GC.collection_threshold = candidate;
			}
		}
		
		uint8_t& flags_of(void* ptr) {
			return GC.object_flags[allocator.index_of_object(ptr)];
		}
		
		void scan_free_list() {
			for (auto it = allocator.free_list_begin(); it != allocator.free_list_end(); ++it) {
				flags_of(*it) |= GCFreed;
			}
		}
		
		void scan_definite_value(VALUE);
		
		void scan_definite_object(Object* object, uint8_t& flags) {
			if (flags & GCFreed) return;
			bool was_scanned = !!(flags & GCReachable);
			flags |= GCReachable;
			
			if (!was_scanned) {
				scan_definite_value(object->cls);
				for (size_t i = 0; i < object->num_alloc_members; ++i) {
					scan_definite_value(object->members[i]);
				}
				const Type* type = object->type;
				if (type != NULL && type->gc_each_root != NULL) {
					type->gc_each_root(object_get_private(object, type), scan_definite_value);
				}
			}
		}
		
		void scan_definite_object(Object* object) {
			scan_definite_object(object, flags_of(object));
		}
		
		void scan_definite_value(VALUE val) {
			if (is_object(val)) {
				scan_definite_object((Object*)val);
			}
		}
		
		void scan_external_roots() {
			for (auto it = external_roots.begin(); it != external_roots.end(); ++it) {
				scan_definite_value(**it);
			}
		}
		
		void scan_possible_value(VALUE val) {
			if (is_object(val)) {
				size_t flag_index;
				Object* object = allocator.find_object_and_index(val, flag_index);
				if (object != NULL) {
					scan_definite_object(object, GC.object_flags[flag_index]);
				}
			}
		}
		
		bool looks_like_value(VALUE val) {
			return is_object(val);
		}
		
		void scan_stack(const void* stack_bottom) {
			ASSERT(stack_bottom < GC.stack_top);
			stack_bottom = (const void*)((uintptr_t)stack_bottom & (UINTPTR_MAX - 0xf)); // align
			const VALUE* begin = (const VALUE*)stack_bottom;
			const VALUE* end = (const VALUE*)GC.stack_top;
			for (const VALUE* p = begin; p < end; ++p) {
				if (looks_like_value(*p)) {
					scan_possible_value(*p);
				}
			}
		}

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
			++GC.num_objects;
			return obj;
		}

		void free_object(Object* obj) {
			const Type* type = obj->type;
			if (type != NULL) {
				type->finalize(object_get_private(obj, type));
				if (type->data_size + sizeof(Object) > SN_CACHE_LINE_SIZE) {
					void* heap_data = *(void**)(obj + 1);
					delete[] reinterpret_cast<byte*>(heap_data);
				}
			}
			snow::dealloc_range(obj->members, obj->num_alloc_members);
			allocator.free(obj);
			--GC.num_objects;
		}
		
		void free_unreachable() {
			for (size_t i = 0; i < GC.num_object_flags; ++i) {
				uint8_t& flags = GC.object_flags[i];
				if (!(flags & GCFreed) && !(flags & GCReachable)) {
					Object* obj = allocator.object_at_index(i);
					free_object(obj);
				}
			}
		}
	}
	
	void init_gc(void** stk_top) {
		GC.num_objects = 0;
		GC.collection_threshold = Allocator::OBJECTS_PER_BLOCK * 4;
		GC.stack_top = stk_top;
		GC.stack_bottom = NULL;
		GC.object_flags = NULL;
		GC.num_object_flags = 0;
	}
	
	void gc() {
		start_collection();
		ssize_t num_before = GC.num_objects;
		scan_free_list();
		scan_external_roots();
		void* sp = NULL;
		scan_stack(&sp);
		free_unreachable();
		adjust_collection_threshold();
		ssize_t num_after = GC.num_objects;
		finish_collection();
		
		fprintf(stderr, "GC: Collection freed %lu/%lu objects, new threshold: %lu.\n", num_before - num_after, num_before, GC.collection_threshold);
	}
	
	Value* gc_create_root(Value initial_value) {
		Value* root = new Value(initial_value);
		external_roots.push_back(root);
		return root;
	}
	
	Value gc_free_root(Value* root) {
		Value v = *root;
		auto it = std::find(external_roots.begin(), external_roots.end(), root);
		external_roots.erase(it);
		return v;
	}
	
	Object* gc_allocate_object(const Type* type) {
		if (GC.num_objects >= GC.collection_threshold) {
			snow::gc();
		}
		Object* obj = allocate_object(type);
		ASSERT(((intptr_t)obj & 0xf) == 0); // unaligned object allocation!
		return obj;
	}
}
