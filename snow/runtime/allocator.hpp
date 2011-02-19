#pragma once
#ifndef ALLOCATOR_HPP_UZ4GOM1J
#define ALLOCATOR_HPP_UZ4GOM1J

#include "snow/basic.h"
#include "snow/object.h"
#include <vector>

namespace snow {
	class Allocator {
	public:
		static const size_t OBJECTS_PER_PAGE = SN_MEMORY_PAGE_SIZE / SN_OBJECT_SIZE;

		struct GCObject : SnObjectBase {
			byte _[SN_OBJECT_SIZE - sizeof(SnObjectBase)];
		};
		
		struct Page {
			union {
				void* memory;
				GCObject* begin;
			};
			GCObject* end;
			GCObject** free_list_head;
			size_t free_list_size;
			
			bool contains(SnObjectBase* ptr) const { return ptr >= begin && ptr < end; }
			size_t num_allocated() const { return (end - begin) - free_list_size; }
			size_t num_available() const { return OBJECTS_PER_PAGE - num_allocated(); }
		};
		
		SnObjectBase* allocate();
		void free(SnObjectBase* object);
		void free(SnObjectBase* object, Page* page);
		bool is_allocated(SnObjectBase* object) const;
		size_t get_num_pages() const { return _pages.size(); }
		Page* get_page(unsigned i) { return &_pages[i]; }
		Page* get_page_for_object(SnObjectBase* object);
	private:
		std::vector<Page> _pages;
		
		Page* find_available_page();
		Page* create_page();
		SnObjectBase* allocate_from_page(Page* page);
	};
}

#endif /* end of include guard: ALLOCATOR_HPP_UZ4GOM1J */
