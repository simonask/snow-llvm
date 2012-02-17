#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <libunwind.h>

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
			printf("ip = %lx, sp = %lx\n", (long)ip, (long)sp);
		}
		printf("ip = %lx\n", (long)ip);
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
		
		//display_stack_trace(1);
		
		throw snow::Exception(ex);
	}
}
