#include "snow/basic.h"
#include "snow/module.hpp"
#include "snow/object.hpp"
#include "snow/class.hpp"
#include "snow/objectptr.hpp"
#include "snow/function.hpp"
#include "snow/snow.hpp"
#include "snow/boolean.hpp"
#include "snow/numeric.hpp"
#include "snow/str-format.hpp"
#include "snow/exception.hpp"
#include "snow/gc.hpp"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

namespace snow_io {
	using namespace snow;
	
	struct IO {
		FILE* fp;
		int fd;
		
		IO() : fp(NULL), fd(-1) {}
		~IO() { close(); }
		
		void init_with_file(int fd, FILE* fp) {
			this->fd = fd;
			this->fp = fp;
		}
		
		void init_with_file(int fd, const char* mode) {
			this->fd = fd;
			this->fp = fdopen(fd, mode);
		}
		
		void init_with_file(FILE* fp) {
			this->fp = fp;
			this->fd = fileno(fp);
		}
		
		bool has_fp() const { return fp != NULL; }
		bool has_fd() const { return fd != -1; }
		
		bool close() {
			if (has_fp()) {
				::fclose(fp);
				fp = NULL;
				fd = -1;
				return true;
			} else if (has_fd()) {
				::close(fd);
				fd = -1;
				return true;
			}
			return false;
		}
		
		ssize_t write(const char* buffer, size_t len) {
			if (has_fp()) {
				return ::fwrite(buffer, 1, len, fp);
			} else if (has_fd()) {
				return ::write(fd, buffer, len);
			}
			return -1;
		}
		
		ssize_t read(char* buffer, size_t max_len) {
			if (has_fp()) {
				return ::fread(buffer, 1, max_len, fp);
			} else if (has_fd()) {
				return ::read(fd, buffer, max_len);
			}
			return -1;
		}
		
		bool eof() const {
			if (has_fp()) {
				return feof(fp) != 0;
			}
			return false; // eof not supported
		}
		
		void flush() {
			if (has_fp()) {
				fflush(fp);
			}
		}
	};
	
	namespace {
		VALUE IO_close(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			return boolean_to_value(io->close());
		}
		
		VALUE IO_write(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			ObjectPtr<String> str = value_to_string(it);
			if (str != NULL) {
				size_t len = string_size(str);
				char buf[len];
				len = string_copy_to(str, buf, len);
				ssize_t n = io->write(buf, len);
				return integer_to_value(n);
			}
			throw_exception_with_description("Could not convert object to string: %@.", value_inspect(it));
			return SN_NIL;
		}
		
		VALUE IO_read(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			if (!is_integer(it)) {
				throw_exception_with_description("IO#read expects 1 argument: number of bytes.");
			}
			int64_t n = value_to_integer(it);
			PreallocatedStringData data;
			preallocate_string_data(data, n);
			ssize_t nr = io->read(data.data, n);
			data.size = nr;
			return create_string_from_preallocated_data(data);
		}
		
		VALUE IO_is_eof(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			return boolean_to_value(io->eof());
		}
		
		VALUE IO_fcntl(const CallFrame* here, VALUE self, VALUE it) {
			throw_exception_with_description("IO#fcntl: Not implemented yet.");
			return NULL;
		}
		
		VALUE IO_flush(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			io->flush();
			return SN_NIL;
		}
		
		VALUE IO_inspect(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			return format_string("[%@@%@ fd:%@ fp:%@]", class_get_name(get_class(io)), format::pointer(self), io->fd, format::pointer(io->fp));
		}
		
		VALUE IO_puts(const CallFrame* here, VALUE self, VALUE it) {
			Value args[] = { it };
			call_method(self, sym("write"), 1, args);
			args[0] = create_string_constant("\n");
			call_method(self, sym("write"), 1, args);
			return SN_NIL;
		}
		
		VALUE IO_reopen(const CallFrame* here, VALUE self, VALUE it) {
			throw_exception_with_description("IO#reopen: Not implemented yet.");
			return NULL;
		}
		
		VALUE IO_seek(const CallFrame* here, VALUE self, VALUE it) {
			throw_exception_with_description("IO#seek: Not implemented yet.");
			return NULL;
		}
		
		VALUE IO_tell(const CallFrame* here, VALUE self, VALUE it) {
			throw_exception_with_description("IO#seek: Not implemented yet.");
			return NULL;
		}
		
		VALUE IO_get_async(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			int flags = fcntl(io->fd, F_GETFL);
			return boolean_to_value((flags & O_NONBLOCK) != 0);
		}
		
		VALUE IO_set_async(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<IO> io = self;
			bool set = is_truthy(it);
			int flags = fcntl(io->fd, F_GETFL);
			if (set)
				flags |= O_NONBLOCK;
			else
				flags &= ~O_NONBLOCK;
			int r = fcntl(io->fd, F_SETFL, flags);
			if (r == -1) {
				throw_exception_with_description("IO#set_async: fcntl(): %@", strerror(errno));
			}
			return boolean_to_value(set);
		}
		
		ObjectPtr<Class> get_io_class() {
			static Value* root = NULL;
			if (!root) {
				ObjectPtr<Class> io = create_class_for_type(sym("IO"), get_type<IO>());
				SN_DEFINE_METHOD(io, "close", IO_close);
				SN_DEFINE_METHOD(io, "write", IO_write);
				SN_DEFINE_METHOD(io, "<<", IO_write);
				SN_DEFINE_METHOD(io, "read", IO_read);
				SN_DEFINE_PROPERTY(io, "eof?", IO_is_eof, NULL);
				SN_DEFINE_METHOD(io, "fcntl", IO_fcntl);
				SN_DEFINE_METHOD(io, "flush", IO_flush);
				SN_DEFINE_METHOD(io, "inspect", IO_inspect);
				SN_DEFINE_METHOD(io, "puts", IO_puts);
				SN_DEFINE_METHOD(io, "reopen", IO_reopen);
				SN_DEFINE_METHOD(io, "seek", IO_seek);
				SN_DEFINE_METHOD(io, "tell", IO_tell);
				SN_DEFINE_PROPERTY(io, "async", IO_get_async, IO_set_async);
				SN_DEFINE_PROPERTY(io, "nonblock", IO_get_async, IO_set_async);
				root = gc_create_root(io);
			}
			return *root;
		}
		
		VALUE get_io_class(const CallFrame* here, VALUE self, VALUE it) {
			return get_io_class();
		}
		
		ObjectPtr<IO> create_io_for_file(FILE* fp) {
			ObjectPtr<IO> io = create_object_without_initialize(get_io_class());
			io->init_with_file(fp);
			object_initialize(io, NULL);
			return io;
		}
		
		ObjectPtr<Class> get_file_class();
		
		ObjectPtr<IO> create_file_for_file(FILE* fp) {
			ObjectPtr<IO> io = create_object_without_initialize(get_file_class());
			io->init_with_file(fp);
			object_initialize(io, NULL);
			return io;
		}
		
		VALUE File_open(const CallFrame* here, VALUE self, VALUE it) {
			ObjectPtr<String> filename = it;
			if (filename == NULL) {
				throw_exception_with_description("File.open: Expected 1st argument to be filename, got %@.", value_inspect(it));
			}
			ObjectPtr<String> mode;
			if (here->args && here->args->size >= 2) {
				mode = here->args->data[1];
				if (mode == NULL) {
					throw_exception_with_description("File.open: Expected 2nd argument to be a mode string, got %@.", value_inspect(here->args->data[1]));
				}
			} else {
				mode = create_string_constant("rw");
			}
			
			size_t zmode_len = string_size(mode);
			char zmode[zmode_len+1];
			string_copy_to(mode, zmode, zmode_len);
			zmode[zmode_len] = '\0';
			
			size_t zfile_len = string_size(filename);
			char zfile[zfile_len+1];
			string_copy_to(filename, zfile, zfile_len);
			zfile[zfile_len] = '\0';
			
			FILE* fp = fopen(zfile, zmode);
			if (fp == NULL) {
				throw_exception_with_description("File.open: fopen(%@, %@): %@", filename, mode, strerror(errno));
			}
			
			ObjectPtr<IO> file = create_file_for_file(fp);
			object_set_instance_variable(file, sym("path"), filename);
			return file;
		}
		
		VALUE File_get_path(const CallFrame* here, VALUE self, VALUE it) {
			return object_get_instance_variable(self, sym("path"));
		}
		
		ObjectPtr<Class> get_file_class() {
			static Value* root = NULL;
			if (root == NULL) {
				ObjectPtr<Class> file = create_class(sym("File"), get_io_class());
				SN_OBJECT_DEFINE_METHOD(file, "open", File_open);
				SN_DEFINE_PROPERTY(file, "path", File_get_path, NULL);
				root = gc_create_root(file);
			}
			return *root;
		}
		
		VALUE get_file_class(const CallFrame* here, VALUE self, VALUE it) {
			return get_file_class();
		}
		
		VALUE get_stdout(const CallFrame* here, VALUE self, VALUE it) {
			static Value* p = NULL;
			if (!p) {
				ObjectPtr<IO> io = create_io_for_file(stdout);
				p = gc_create_root(io);
			}
			return *p;
		}
		
		VALUE get_stderr(const CallFrame* here, VALUE self, VALUE it) {
			static Value* p = NULL;
			if (!p) {
				ObjectPtr<IO> io = create_io_for_file(stderr);
				p = gc_create_root(io);
			}
			return *p;
		}
		
		VALUE get_stdin(const CallFrame* here, VALUE self, VALUE it) {
			static Value* p = NULL;
			if (!p) {
				ObjectPtr<IO> io = create_io_for_file(stdin);
				p = gc_create_root(io);
			}
			return *p;
		}
	}
	
	VALUE module_init() {
		AnyObjectPtr io = create_object(get_object_class(), 0, NULL);
		SN_OBJECT_DEFINE_PROPERTY(io, "stdout", get_stdout, NULL);
		SN_OBJECT_DEFINE_PROPERTY(io, "stderr", get_stderr, NULL);
		SN_OBJECT_DEFINE_PROPERTY(io, "stdin", get_stdin, NULL);
		SN_OBJECT_DEFINE_PROPERTY(io, "IO", get_io_class, NULL);
		SN_OBJECT_DEFINE_PROPERTY(io, "File", get_file_class, NULL);
		return io;
	}
}

CAPI snow::VALUE snow_module_init() {
	return snow_io::module_init();
}

namespace snow {
	SN_REGISTER_TYPE(snow_io::IO, ((Type){ .data_size = sizeof(snow_io::IO), .initialize = snow::construct<snow_io::IO>, .finalize = snow::destruct<snow_io::IO>, .copy = NULL, .gc_each_root = NULL}))
}