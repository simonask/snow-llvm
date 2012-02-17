#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include "codemanager.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <libunwind.h>
#include <cxxabi.h>

namespace snow {
	namespace {
		struct Ensure {
			Ensure(Value ensure_f) : ensure_f(ensure_f) {}
			~Ensure() {
				if (is_truthy(ensure_f))
					snow::call(ensure_f, NULL, 0, NULL);
			}
			Value ensure_f;
		};
	}
	
	Value try_catch_ensure(Value try_f, Value catch_f, Value ensure_f) {
		Ensure ensure(ensure_f);
		Value result = NULL;
		try {
			result = call(try_f, NULL, 0, NULL);
		}
		catch (snow::Exception ex) {
			if (catch_f)
				return call(catch_f, NULL, 1, &ex.value);
			throw ex;
		}
		return result;
	}
	
	void display_stack_trace(int skip) {
		unw_cursor_t cursor;
		unw_context_t uc;
		unw_word_t ip;
		unw_word_t sp;
		
		unw_getcontext(&uc);
		unw_init_local(&cursor, &uc);
		int n = -1; // this function is also counted!
		while (unw_step(&cursor) > 0) {
			unw_get_reg(&cursor, UNW_REG_IP, &ip);
			unw_get_reg(&cursor, UNW_REG_SP, &sp);
			++n;
			if (n < skip) continue;
			
			SourceFile file;
			SourceLocation location;
			if (CodeManager::get()->find_source_location_from_instruction_pointer((void*)ip, file, location)) {
				printf("(SNOW)  %p   %s:%d (col %d)", (void*)ip, file.path.c_str(), location.line, location.column);
			} else {
				char name_buffer[1024];
				unw_word_t offset;
				if (unw_get_proc_name(&cursor, name_buffer, 1024, &offset) == 0) {
					// Try to demangle C++ name
					int status;
					char* demangled = abi::__cxa_demangle(name_buffer, NULL, NULL, &status);
					if (status == 0) {
						printf("(C++)   %p   %s + %llu", (void*)ip, demangled, (uint64_t)offset);
						::free(demangled);
					} else {
						printf("(C)     %p   %s + %llu", (void*)ip, name_buffer, (uint64_t)offset);
					}
				} else {
					printf("        %p", (void*)ip);
				}
			}
			printf("\n");
		}
	}

	void throw_exception(Value ex) {
		// TODO: Backtrace and all that jazz
		
		ObjectPtr<String> str = value_to_string(ex);
		size_t sz = snow::string_size(str);
		char buffer[sz+1];
		snow::string_copy_to(str, buffer, sz);
		buffer[sz] = '\0';
		
		#if defined(DEBUG)
		fprintf(stderr, "SNOW: Throwing exception: %s\n", buffer);
		#endif
		
		display_stack_trace(1);
		
		throw snow::Exception(ex);
	}
}
