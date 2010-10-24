#ifndef LINKBUFFER_H_U08VA9BE
#define LINKBUFFER_H_U08VA9BE

#include "snow/basic.h"

struct SnLinkBufferPage;

typedef struct SnLinkBuffer {
	size_t page_size;
	struct SnLinkBufferPage* head;
	struct SnLinkBufferPage* tail;
} SnLinkBuffer;

CAPI SnLinkBuffer* snow_create_linkbuffer(size_t page_size);
CAPI void snow_init_linkbuffer(SnLinkBuffer*, size_t page_size);
CAPI void snow_free_linkbuffer(SnLinkBuffer*);
CAPI size_t snow_linkbuffer_size(SnLinkBuffer*);
CAPI size_t snow_linkbuffer_push(SnLinkBuffer*, byte);
CAPI size_t snow_linkbuffer_push_string(SnLinkBuffer*, const char*);
CAPI size_t snow_linkbuffer_push_data(SnLinkBuffer*, const byte*, size_t len);
CAPI size_t snow_linkbuffer_copy_data(SnLinkBuffer*, void* dst, size_t n);
CAPI size_t snow_linkbuffer_modify(SnLinkBuffer*, size_t offset, size_t len, byte* new_data);
CAPI void snow_linkbuffer_clear(SnLinkBuffer*);

#endif /* end of include guard: LINKBUFFER_H_U08VA9BE */
