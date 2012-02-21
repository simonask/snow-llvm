#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include "snow/class.hpp"
#include "codemanager.hpp"
#include "snow/numeric.hpp"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <libunwind.h>
#include <cxxabi.h>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace snow {
	namespace {
		enum TracePointType : int8_t {
			TracePointTypeUnknown = -1,
			TracePointTypeNativeC,
			TracePointTypeNativeCPP,
			TracePointTypeSnow
		};
		
		struct TracePoint {
			struct DebugInfo {
				ObjectPtr<const Environment> environment;
				           const SourceFile* file;
				       const SourceLocation* location;
			};
			
			TracePointType type;
			void* ip;
			std::string symbol;
			std::unique_ptr<DebugInfo> info;
			
			TracePoint(void* ip) : type(TracePointTypeUnknown), ip(ip) {}
			TracePoint(void* ip, const std::string& symbol, bool is_cpp = true) : type(is_cpp ? TracePointTypeNativeCPP : TracePointTypeNativeC), ip(ip), symbol(symbol) {}
			TracePoint(void* ip, ObjectPtr<const Environment> environment, const SourceFile* file, const SourceLocation* location) : type(TracePointTypeSnow), ip(ip), info(new DebugInfo) {
				info->environment = environment;
				info->location = location;
				info->file = file;
			}
			TracePoint(const TracePoint& other) : type(other.type), ip(other.ip), symbol(other.symbol) {
				if (other.info != nullptr) {
					info = std::unique_ptr<DebugInfo>(new DebugInfo(*other.info));
				}
			}
			TracePoint& operator=(const TracePoint& other) {
				type = other.type;
				ip = other.ip;
				symbol = other.symbol;
				if (other.info != nullptr) {
					info = std::unique_ptr<DebugInfo>(new DebugInfo(*other.info));
				}
				return *this;
			}
		};
	}
	
	struct Exception {
		Value message;
		std::vector<TracePoint> backtrace;
	};
	
	static void exception_gc_each_root(void* priv, GCCallback callback) {
		Exception* ex = (Exception*)priv;
		callback(ex->message);
	}
	
	SN_REGISTER_CPP_TYPE(Exception, exception_gc_each_root);
	
	ObjectPtr<Exception> create_exception(const std::string& message) {
		return create_exception_with_message(create_string_with_size(message.c_str(), message.size()));
	}
	
	ObjectPtr<Exception> create_exception_with_message(Value val) {
		return create_object(get_exception_class(), 1, &val);
	}
	
	namespace bindings {
		VALUE exception_initialize(const CallFrame* here, VALUE self, VALUE it) {
			ExceptionPtr ex = self;
			ex->message = it;
			return ex;
		}
		
		VALUE exception_get_message(const CallFrame* here, VALUE self, VALUE it) {
			ExceptionPtr ex = self;
			return ex->message;
		}
		
		VALUE exception_get_backtrace(const CallFrame* here, VALUE self, VALUE it) {
			ExceptionPtr ex = self;
			return snow::exception_get_backtrace(ex);
		}
		
		VALUE exception_get_internal_backtrace(const CallFrame* here, VALUE self, VALUE it) {
			ExceptionPtr ex = self;
			return snow::exception_get_internal_backtrace(ex);
		}
		
		VALUE exception_to_string(const CallFrame* here, VALUE self, VALUE it) {
			ExceptionPtr ex = self;
			return value_to_string(ex->message);
		}
		
		VALUE exception_get_source_excerpt(const CallFrame* here, VALUE self, VALUE it) {
			return snow::exception_get_source_excerpt(self);
		}
		
		VALUE exception_get_line(const CallFrame* here, VALUE self, VALUE it) {
			return snow::exception_get_line(self);
		}
		
		VALUE exception_get_column(const CallFrame* here, VALUE self, VALUE it) {
			return snow::exception_get_column(self);
		}
	}
	
	ObjectPtr<Class> get_exception_class() {
		static Value* root = NULL;
		if (root == NULL) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Exception"), get_type<Exception>());
			SN_DEFINE_METHOD(cls, "initialize", bindings::exception_initialize);
			SN_DEFINE_PROPERTY(cls, "message", bindings::exception_get_message, NULL);
			SN_DEFINE_PROPERTY(cls, "backtrace", bindings::exception_get_backtrace, NULL);
			SN_DEFINE_PROPERTY(cls, "internal_backtrace", bindings::exception_get_internal_backtrace, NULL);
			SN_DEFINE_PROPERTY(cls, "source_excerpt", bindings::exception_get_source_excerpt, NULL);
			SN_DEFINE_PROPERTY(cls, "line", bindings::exception_get_line, NULL);
			SN_DEFINE_PROPERTY(cls, "column", bindings::exception_get_column, NULL);
			SN_DEFINE_METHOD(cls, "to_string", bindings::exception_to_string);
			root = gc_create_root(cls);
		}
		return *root;
	}
	
	Value try_catch_ensure(Value try_f, Value catch_f, Value ensure_f) {
		struct Ensure {
			Ensure(Value ensure_f) : ensure_f(ensure_f) {}
			~Ensure() {
				if (is_truthy(ensure_f))
					snow::call(ensure_f, NULL, 0, NULL);
			}
			Value ensure_f;
		};
		
		Ensure ensure(ensure_f);
		Value result;
		try {
			if (is_truthy(try_f))
				result = call(try_f, NULL, 0, NULL);
		}
		catch (ExceptionPtr ex) {
			if (is_truthy(catch_f)) {
				Value args[] = { ex };
				return call(catch_f, NULL, 1, args);
			}
			throw ex;
		}
		return result;
	}
	
	static std::vector<TracePoint> build_stack_trace() {
		std::vector<TracePoint> points;
		unw_cursor_t cursor;
		unw_context_t uc;
		unw_word_t wip;

		unw_getcontext(&uc);
		unw_init_local(&cursor, &uc);
		int n = -1; // this function is also counted!
		while (unw_step(&cursor) > 0) {
			++n;
			if (n < 1) continue;
				
			unw_get_reg(&cursor, UNW_REG_IP, &wip);
			void* ip = (void*)wip;
			const SourceFile* file;
			const SourceLocation* location;
			if (CodeManager::get()->find_source_location_from_instruction_pointer(ip, file, location)) {
				CallFrame* frame = CodeManager::get()->find_call_frame(&cursor);
				ObjectPtr<const Environment> environment = call_frame_environment(frame);
				points.emplace_back(ip, environment, file, location);
			} else {
				static const size_t NAME_BUFFER_LEN = 512; // some C++ symbols can be really long!
				char name_buffer[NAME_BUFFER_LEN];
				name_buffer[NAME_BUFFER_LEN-1] = '\0';
					
				unw_word_t offset;
				if (unw_get_proc_name(&cursor, name_buffer, NAME_BUFFER_LEN, &offset) == 0) {
					// Try to demangle C++ name
					bool is_cpp = false;
					int status;
					char* demangled = abi::__cxa_demangle(name_buffer, NULL, NULL, &status);
					if (status == 0) {
						::strncpy(name_buffer, demangled, NAME_BUFFER_LEN-1);
						::free(demangled);
						is_cpp = true;
					}
					points.emplace_back(ip, name_buffer, is_cpp);
					
					std::string& symbol = points.back().symbol;
					// replace common std class names with more readable versions
					std::string::size_type pos;
					
					static const char* const replacements[] = {
						"std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >", "std::string",
					};
					
					for (size_t i = 0; i < countof(replacements)/2; ++i) {
						std::string needle = replacements[i*2];
						std::string replacement = replacements[i*2+1];
						std::string::size_type pos = 0;
						while ((pos = symbol.find(needle, pos)) != std::string::npos) {
							symbol.replace(pos, needle.size(), replacement);
						}
					}
				} else {
					points.emplace_back(ip);
				}
			}
		}
		
		return points;
	}
	
	using std::setw;
	using std::setfill;
	using std::setprecision;
	using std::hex;
	using std::dec;
	using std::left;
	using std::right;
	
	static StringPtr backtrace_to_string(const std::vector<TracePoint>& backtrace, ssize_t limit, bool omit_native) {
		std::stringstream ss;
		
		ssize_t n = 0;
		bool limit_reached = false;
		for (const TracePoint& point: backtrace) {
			if (omit_native && point.type != TracePointTypeSnow)
				continue;
			if (limit >= 0 && n > limit) {
				limit_reached = true;
				break;
			}
			
			ss << "0x" << setw(16) << setfill('0') << right << hex << (uintptr_t)point.ip;
			ss << "    ";
			switch (point.type) {
				case TracePointTypeUnknown:
					break;
				case TracePointTypeNativeC:
				case TracePointTypeNativeCPP: {
					ss << point.symbol;
					break;
				}
				case TracePointTypeSnow: {
					ss << point.info->file->path << ":" << dec << point.info->location->line << ":" << point.info->location->column;
					ss << " (";
					Value self = environment_get_self(point.info->environment);
					ObjectPtr<const Array> args = environment_get_arguments(point.info->environment);
					ObjectPtr<const String> str = format_string("self = %@, arguments = %@", value_inspect(self), value_inspect(args));
					string_copy_to(str, ss);
					ss << ")";
					break;
				}
			}
			ss << '\n';
			++n;
		}
		
		if (limit_reached) {
			ss << "...\n";
		}
		
		return create_string_with_size(ss.str().c_str(), ss.str().size());
	}

	void throw_exception(Value value) {
		ExceptionPtr ex = value;
		if (ex == nullptr) {
			// It's not already an exception, so wrap it in one.
			ex = create_exception_with_message(value);
		}
		
		if (ex->backtrace.size() == 0) {
			ex->backtrace = build_stack_trace();
		}
		
		throw ex;
	}
	
	StringPtr exception_get_internal_backtrace(ExceptionConstPtr ex, ssize_t limit) {
		return backtrace_to_string(ex->backtrace, limit, false);
	}
	
	StringPtr exception_get_backtrace(ExceptionConstPtr ex, ssize_t limit) {
		return backtrace_to_string(ex->backtrace, limit, true);
	}
	
	StringPtr exception_get_source_excerpt(ExceptionConstPtr ex, size_t radius) {
		static const size_t TAB_WIDTH = 4;
		std::stringstream ss;
		
		const TracePoint* point = nullptr;
		for (const TracePoint& p: ex->backtrace) {
			if (p.type == TracePointTypeSnow) {
				point = &p;
				break;
			}
		}
		if (point != nullptr) {
			const SourceLocation& location = *point->info->location;
			const SourceFile& file = *point->info->file;
			
			ssize_t begin_line = (ssize_t)location.line - (ssize_t)radius;
			if (begin_line < 0) begin_line = 0;
			ssize_t end_line = location.line;
			
			std::istringstream ifs(file.source);
			std::string line;
			size_t lineno = 1;
			size_t adjusted_column = 0;
			while (std::getline(ifs, line)) {
				if (lineno >= begin_line && lineno <= end_line) {
					// line number column
					ss << setfill('0') << setw(6) << right << lineno;
					ss << "    ";
					size_t column = location.column;
					for (size_t i = 0; i < line.size(); ++i) {
						if (line[i] == '\t') {
							for (size_t j = 0; j < TAB_WIDTH; ++j)
								ss << ' ';
							if (i < location.column) {
								column += TAB_WIDTH-1;
							}
						} else {
							ss << line[i];
						}
					}
					if (line.back() != '\n')
						ss << '\n';
					if (lineno == location.line) {
						adjusted_column = column;
					}
				}
				++lineno;
			}
			
			// print squiggly column indicator
			for (size_t i = 0; i < adjusted_column+9; ++i) {
				ss << '~';
			}
			ss << "^\n";
		} else {
			return nullptr;
		}
		
		return create_string_with_size(ss.str().c_str(), ss.str().size());
	}
	
	StringPtr exception_get_file(ExceptionConstPtr ex) {
		const TracePoint* point = nullptr;
		for (const TracePoint& p: ex->backtrace) {
			if (p.type == TracePointTypeSnow) {
				point = &p;
				break;
			}
		}
		if (point != nullptr) {
			const std::string& path = point->info->file->path;
			return create_string_with_size(path.c_str(), path.size());
		}
		return nullptr;
	}
	
	Immediate exception_get_line(ExceptionConstPtr ex) {
		const TracePoint* point = nullptr;
		for (const TracePoint& p: ex->backtrace) {
			if (p.type == TracePointTypeSnow) {
				point = &p;
				break;
			}
		}
		if (point != nullptr)
			return integer_to_value((int64_t)point->info->location->line);
		return nullptr;
	}
	
	Immediate exception_get_column(ExceptionConstPtr ex) {
		const TracePoint* point = nullptr;
		for (const TracePoint& p: ex->backtrace) {
			if (p.type == TracePointTypeSnow) {
				point = &p;
				break;
			}
		}
		if (point != nullptr)
			return integer_to_value((int64_t)point->info->location->column);
		return nullptr;
	}
}
