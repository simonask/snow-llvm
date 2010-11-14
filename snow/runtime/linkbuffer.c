#include "snow/linkbuffer.h"
#include "intern.h"
#include <stdlib.h>
#include <string.h>


// TODO: Use snow::LinkBuffer<T>

typedef struct SnLinkBufferPage {
	struct SnLinkBufferPage* next;
	size_t offset;
	byte data[];
} SnLinkBufferPage;


SnLinkBuffer* snow_create_linkbuffer(size_t page_size) {
	SnLinkBuffer* buf = (SnLinkBuffer*)malloc(sizeof(SnLinkBuffer));
	snow_init_linkbuffer(buf, page_size);
	return buf;
}

void snow_init_linkbuffer(SnLinkBuffer* buf, size_t page_size) {
	buf->page_size = page_size;
	buf->head = NULL;
	buf->tail = NULL;
}

void snow_free_linkbuffer(SnLinkBuffer* buf) {
	snow_linkbuffer_clear(buf);
	free(buf);
}

size_t snow_linkbuffer_size(SnLinkBuffer* buf) {
	size_t size = 0;
	SnLinkBufferPage* page = buf->head;
	while (page) {
		if (page->next)
			size += buf->page_size;
		else
			size += page->offset;
		page = page->next;
	}
	return size;
}

size_t snow_linkbuffer_push(SnLinkBuffer* buf, byte b) {
	if (!buf->tail || buf->tail->offset == buf->page_size) {
		SnLinkBufferPage* old_tail = buf->tail;
		buf->tail = (SnLinkBufferPage*)malloc(sizeof(SnLinkBufferPage) + buf->page_size);
		buf->tail->next = NULL;
		buf->tail->offset = 0;
		if (!buf->head)
			buf->head = buf->tail;
		if (old_tail)
			old_tail->next = buf->tail;
	}
	
	buf->tail->data[buf->tail->offset++] = b;
	return 1;
}

size_t snow_linkbuffer_push_string(SnLinkBuffer* buf, const char* c) {
	size_t n = 0;
	while (*c) {
		n += snow_linkbuffer_push(buf, *c++);
	}
	return n;
}

size_t snow_linkbuffer_push_data(SnLinkBuffer* buf, const byte* data, size_t len) {
	size_t n = 0;
	for (size_t i = 0; i < len; ++i) {
		n += snow_linkbuffer_push(buf, data[i]);
	}
	return n;
}

size_t snow_linkbuffer_copy_data(SnLinkBuffer* buf, void* _dst, size_t n) {
	SnLinkBufferPage* current = buf->head;
	size_t copied = 0;
	byte* dst = (byte*)_dst;
	while (current && copied < n) {
		size_t remaining = n - copied; // remaining space in dst
		size_t to_copy = remaining < current->offset ? remaining : current->offset;
		memcpy(&dst[copied], current->data, to_copy);
		copied += to_copy;
		current = current->next;
	}
	return copied;
}

size_t snow_linkbuffer_modify(SnLinkBuffer* buf, size_t offset, size_t len, byte* new_data) {
	size_t buffer_offset = 0;
	size_t copied = 0;
	SnLinkBufferPage* current = buf->head;
	while (current && copied < len) {
		size_t next_offset = offset + copied;
		if (next_offset >= buffer_offset && next_offset < buffer_offset + current->offset)
		{
			size_t page_offset = next_offset - buffer_offset;
			size_t remaining = current->offset - page_offset; // remaining space in current page
			size_t to_copy = remaining < len ? remaining : len;
			memcpy(&current->data[page_offset], &new_data[copied], to_copy);
			copied += to_copy;
		}
		
		buffer_offset += current->offset;
		current = current->next;
	}
	return copied;
}

void snow_linkbuffer_clear(SnLinkBuffer* buf) {
	SnLinkBufferPage* page = buf->head;
	while (page) {
		SnLinkBufferPage* next = page->next;
		free(page);
		page = next;
	}
	buf->head = NULL;
	buf->tail = NULL;
}