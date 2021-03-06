#include "eh-frame.hpp"
#include "dwarf.hpp"

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#define USE_KEYMGR_HACK
#endif
#endif

extern "C" {
	void __register_frame(void*);
	void __deregister_frame(void*);
}

namespace snow {
	#if defined(USE_KEYMGR_HACK)
	namespace {
		// THE FOLLOWING HACK IS STOLEN FROM LLVM'S JIT.cpp:
		
		// libgcc defines the __register_frame function to dynamically register new
		// dwarf frames for exception handling. This functionality is not portable
		// across compilers and is only provided by GCC. We use the __register_frame
		// function here so that code generated by the JIT cooperates with the unwinding
		// runtime of libgcc. When JITting with exception handling enable, LLVM
		// generates dwarf frames and registers it to libgcc with __register_frame.
		//
		// The __register_frame function works with Linux.
		//
		// Unfortunately, this functionality seems to be in libgcc after the unwinding
		// library of libgcc for darwin was written. The code for darwin overwrites the
		// value updated by __register_frame with a value fetched with "keymgr".
		// "keymgr" is an obsolete functionality, which should be rewritten some day.
		// In the meantime, since "keymgr" is on all libgccs shipped with apple-gcc, we
		// need a workaround in LLVM which uses the "keymgr" to dynamically modify the
		// values of an opaque key, used by libgcc to find dwarf tables.
		
		struct LibgccObject {
			void* unused1;
			void* unused2;
			void* unused3;
			void* frame; // fde
			union { // encoding?
				struct {
					unsigned long sorted : 1;
					unsigned long from_array : 1;
					unsigned long mixed_encoding : 1;
					unsigned long encoding : 8;
					unsigned long count : 21;
				} b;
				size_t i;
			} encoding;
			char* fde_end;
			LibgccObject* next;
		};
		
		extern "C" {
			void _keymgr_set_and_unlock_processwide_ptr(int, void*);
			void* _keymgr_get_and_lock_processwide_ptr(int);
		}
		
		static const int KEYMGR_GCC3_DW2_OBJ_LIST = 302; // Dwarf2 object list
		
		struct LibgccObjectInfo {
			LibgccObject* seen_objects;
			LibgccObject* unseen_objects;
			unsigned unused[2];
		};
		
		void darwin_register_frame(void* frame_begin) {
			LibgccObjectInfo* object_info = (LibgccObjectInfo*)_keymgr_get_and_lock_processwide_ptr(KEYMGR_GCC3_DW2_OBJ_LIST);
			if (object_info == NULL) {
				object_info = (LibgccObjectInfo*)malloc(sizeof(LibgccObjectInfo));
				object_info->seen_objects = NULL;
				object_info->unseen_objects = NULL;
			}
			ASSERT(object_info != NULL);
			LibgccObject* object = (LibgccObject*)malloc(sizeof(LibgccObject)); // Cannot deallocate, potential leak?
			
			// LLVM says this is what libgcc does:
			object->unused1 = (void*)-1;
			object->unused2 = 0;
			object->unused3 = 0;
			object->frame = frame_begin;
			object->encoding.i = 0;
			object->encoding.b.encoding = dwarf::DW_EH_PE_omit;
			object->fde_end = (char*)object_info->unseen_objects;
			object->next = object_info->unseen_objects;
			object_info->unseen_objects = object;
			_keymgr_set_and_unlock_processwide_ptr(KEYMGR_GCC3_DW2_OBJ_LIST, object_info);
		}
	}
	#endif // USE_KEYMGR_HACK
	
	void register_frame(void* frame_begin) {
		#if defined(USE_KEYMGR_HACK)
		darwin_register_frame(frame_begin);
		#else
		__register_frame(frame_begin);
		#endif
	}
	
	void deregister_frame(void* frame_begin) {
		#if !defined(USE_KEYMGR_HACK)
		__deregister_frame(frame_begin);
		#endif
	}
}