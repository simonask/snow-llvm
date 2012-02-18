#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include "snow/class.hpp"
#include "codemanager.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <libunwind.h>
#include <cxxabi.h>
#include <sstream>
#include <iomanip>

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
				     const CallFrame* frame;
				    const SourceFile* file;
				const SourceLocation* location;
			};
			
			TracePointType type;
			void* ip;
			std::string symbol;
			std::unique_ptr<DebugInfo> info;
			
			TracePoint(void* ip) : type(TracePointTypeUnknown), ip(ip) {}
			TracePoint(void* ip, const std::string& symbol, bool is_cpp = true) : type(is_cpp ? TracePointTypeNativeCPP : TracePointTypeNativeC), ip(ip), symbol(symbol) {}
			TracePoint(void* ip, const CallFrame* frame, const SourceFile* file, const SourceLocation* location) : type(TracePointTypeSnow), ip(ip), info(new DebugInfo) {
				info->frame = frame;
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
	}
	
	ObjectPtr<Class> get_exception_class() {
		static Value* root = NULL;
		if (root == NULL) {
			ObjectPtr<Class> cls = create_class_for_type(snow::sym("Exception"), get_type<Exception>());
			SN_DEFINE_METHOD(cls, "initialize", bindings::exception_initialize);
			SN_DEFINE_PROPERTY(cls, "message", bindings::exception_get_message, NULL);
			SN_DEFINE_PROPERTY(cls, "backtrace", bindings::exception_get_backtrace, NULL);
			SN_DEFINE_PROPERTY(cls, "internal_backtrace", bindings::exception_get_internal_backtrace, NULL);
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
				const CallFrame* frame = CodeManager::get()->find_call_frame(&cursor);
				points.emplace_back(ip, frame, file, location);
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
				} else {
					points.emplace_back(ip);
				}
			}
		}
		
		return points;
	}
	
	static StringPtr backtrace_to_string(const std::vector<TracePoint>& backtrace, bool omit_native) {
		std::stringstream ss;
		
		using std::setw;
		using std::setfill;
		using std::setprecision;
		using std::hex;
		using std::dec;
		using std::left;
		using std::right;
		
		for (const TracePoint& point: backtrace) {
			if (omit_native && point.type != TracePointTypeSnow)
				continue;
			
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
					ss << point.info->file->path << ":" << point.info->location->line << ":" << point.info->location->column;
					break;
				}
			}
			ss << '\n';
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
	
	StringPtr exception_get_internal_backtrace(ExceptionConstPtr ex) {
		return backtrace_to_string(ex->backtrace, false);
	}
	
	StringPtr exception_get_backtrace(ExceptionConstPtr ex) {
		return backtrace_to_string(ex->backtrace, true);
	}
}
