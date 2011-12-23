#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include <stdio.h>
#include <stdlib.h>

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

	void throw_exception(Value ex) {
		// TODO: Backtrace and all that jazz
		
		ObjectPtr<String> str = value_to_string(ex);
		size_t sz = snow::string_size(str);
		char buffer[sz+1];
		snow::string_copy_to(str, buffer, sz);
		buffer[sz] = '\0';
		
		fprintf(stderr, "SNOW: Throwing exception: %s\n", buffer);
		#if DEBUG
		TRAP();
		#endif
		throw snow::Exception(ex);
	}

	void throw_exception_with_description(const char* fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		ObjectPtr<String> ex = snow::string_format_va(fmt, ap);
		va_end(ap);
		throw_exception(ex);
	}
}
