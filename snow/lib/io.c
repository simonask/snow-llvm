#include "snow/snow.h"
#include "snow/object.h"
#include "snow/str.h"
#include "snow/function.h"
#include "snow/type.h"
#include "snow/pointer.h"
#include "snow/numeric.h"
#include "snow/boolean.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static SnObject* File_proto = NULL;
static SnObject* File = NULL;

static void* fp_copy(const void* fp) {
	return (void*)fp;
}

static void fp_free(void* fp) {
	// TODO: Do the right thing here
}

static SnObject* wrap_fp(FILE* fp, bool builtin) {
	SnObject* file = snow_create_object(File_proto);
	SnPointer* f;
	if (builtin) {
		f = snow_wrap_pointer(fp, NULL, NULL);
	} else {
		f = snow_wrap_pointer(fp, fp_copy, fp_free);
	}
	snow_object_set_member(file, file, snow_sym("_fp"), f);
	return file;
}

static VALUE io_file_initialize(SnCallFrame* here, VALUE self, VALUE it) {
	VALUE vfilename = snow_arguments_get_by_index(here->arguments, 0);
	VALUE vmode = snow_arguments_get_by_index(here->arguments, 1);
	
	if (!vfilename) {
		// TODO: Exception
		fprintf(stderr, "ERROR: File.initialize takes at least one parameter.");
		return NULL;
	}
	
	SnString* filename = snow_value_to_string(vfilename);
	const char* mode = vmode ? snow_string_cstr(snow_value_to_string(vmode)) : "r";
	FILE* fp = fopen(snow_string_cstr(filename), mode);
	if (fp) {
		SnPointer* fp = snow_wrap_pointer(fp, fp_copy, fp_free);
		snow_set_member(self, snow_sym("_fp"), fp);
		return self;
	} else {
		fprintf(stderr, "ERROR: Could not open file.\n");
		return NULL;
		// TODO: Exception
	}
}

static VALUE io_file_read(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	if (it && snow_is_integer(it)) {
		int64_t n = snow_value_to_integer(it);
		char* buffer = (char*)malloc(n+1);
		fread(buffer, n, 1, fp);
		buffer[n] = '\0';
		return snow_create_string_take_ownership(buffer);
	} else {
		SnString* result = snow_create_empty_string();
		char buffer[1025];
		while (!feof(fp)) {
			int n = fread(buffer, 1024, 1, fp);
			buffer[n] = '\0';
			snow_string_append_cstr(result, buffer);
		}
		return result;
	}
}

static VALUE io_file_gets(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	if (!it || !snow_is_integer(it)) {
		// TODO: Exception
		fprintf(stderr, "ERROR: File.gets needs argument max_size.\n");
		return NULL;
	}
	
	int64_t n = snow_value_to_integer(it);
	char* buffer = (char*)malloc(n);
	fgets(buffer, n, fp);
	buffer[n] = '\0';
	return snow_create_string_take_ownership(buffer);
}

static VALUE io_file_write(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	// TODO: Deal with binary data.
	
	int bytes = 0;
	for (size_t i = 0; i < snow_arguments_size(here->arguments); ++i) {
		VALUE v = snow_arguments_get_by_index(here->arguments, i);
		SnString* str = snow_value_to_string(v);
		const char* cstr = snow_string_cstr(str);
		size_t len = snow_string_size(str);
		fwrite(cstr, len, 1, fp);
	}
	
	return bytes ? snow_integer_to_value(bytes) : SN_NIL;
}

static VALUE io_file_seek(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	if (!it || !snow_is_integer(it)) {
		// TODO: Exception
		fprintf(stderr, "ERROR: File.seek needs argument 'position'.\n");
		return NULL;
	}
	
	int64_t n = snow_value_to_integer(it);
	int r = fseek(fp, n, SEEK_SET);
	return snow_boolean_to_value(r == 0);
}

static VALUE io_file_tell(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	long p = ftell(fp);
	return snow_integer_to_value(p);
}

static VALUE io_file_puts(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	for (size_t i = 0; i < snow_arguments_size(here->arguments); ++i) {
		VALUE v = snow_arguments_get_by_index(here->arguments, i);
		SnString* str = snow_value_to_string(v);
		fputs(snow_string_cstr(str), fp);
		fputc('\n', fp);
	}
	
	return SN_NIL;
}

static VALUE io_file_close(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	int r = fclose(fp);
	return snow_boolean_to_value(r == 0);
}

static VALUE io_file_eof(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	return snow_boolean_to_value(feof(fp));
}

static VALUE io_file_size(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	int fd = fileno(fp);
	struct stat s;
	fstat(fd, &s);
	return snow_integer_to_value(s.st_size);
}

static VALUE io_file_each_line(SnCallFrame* here, VALUE self, VALUE it) {
	SnPointer* _fp = snow_get_member(self, snow_sym("_fp"));
	ASSERT(snow_type_of(_fp) == SnPointerType);
	FILE* fp = (FILE*)_fp->ptr;
	
	fseek(fp, 0, SEEK_SET);
	size_t len;
	char* line;
	while ((line = fgetln(fp, &len))) {
		VALUE str = snow_create_string_with_size(line, len);
		snow_call(it, NULL, 1, &str);
	}
	return SN_NIL;
}

CAPI VALUE snow_module_init() {
	SnObject* io = snow_create_object(NULL);
	
	File_proto = snow_create_object(NULL);
	SN_DEFINE_METHOD(File_proto, "initialize", io_file_initialize, 2);
	SN_DEFINE_METHOD(File_proto, "read", io_file_read, 1);
	SN_DEFINE_METHOD(File_proto, "gets", io_file_gets, 1);
	SN_DEFINE_METHOD(File_proto, "write", io_file_write, -1);
	SN_DEFINE_METHOD(File_proto, "seek", io_file_seek, 1);
	SN_DEFINE_METHOD(File_proto, "tell", io_file_tell, 0);
	SN_DEFINE_METHOD(File_proto, "print", io_file_write, -1);
	SN_DEFINE_METHOD(File_proto, "puts", io_file_puts, -1);
	SN_DEFINE_METHOD(File_proto, "<<", io_file_write, -1);
	SN_DEFINE_METHOD(File_proto, "close", io_file_close, 0);
	SN_DEFINE_METHOD(File_proto, "each", io_file_each_line, 1);
	SN_DEFINE_PROPERTY(File_proto, "eof?", io_file_eof, NULL);
	SN_DEFINE_PROPERTY(File_proto, "size", io_file_size, NULL);
/*	SN_DEFINE_PROPERTY(File_proto, "readable?", io_file_readable, NULL);
	SN_DEFINE_PROPERTY(File_proto, "writable?", io_file_writable, NULL);
	SN_DEFINE_PROPERTY(File_proto, "seekable?", io_file_seekable, NULL);*/
	File = snow_create_class_for_prototype(snow_sym("File"), File_proto);
	//SN_DEFINE_METHOD(File, "exists?", io_file_exist, 1);
	
	snow_object_set_member(io, io, snow_sym("File"), File);
	snow_object_set_member(io, io, snow_sym("stdout"), wrap_fp(stdout, true));
	snow_object_set_member(io, io, snow_sym("stdin"), wrap_fp(stdin, true));
	snow_object_set_member(io, io, snow_sym("stderr"), wrap_fp(stderr, true));
	
	return io;
}