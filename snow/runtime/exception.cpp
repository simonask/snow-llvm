#include "snow/exception.hpp"
#include "snow/str.hpp"
#include "snow/snow.hpp"
#include <stdio.h>
#include <stdlib.h>

namespace {
	struct Ensure {
		Ensure(VALUE ensure_f) : ensure_f(ensure_f) {}
		~Ensure() {
			if (snow_eval_truth(ensure_f))
				snow_call(ensure_f, NULL, 0, NULL);
		}
		VALUE ensure_f;
	};
}

CAPI {
	
	VALUE snow_try_catch_ensure(VALUE try_f, VALUE catch_f, VALUE ensure_f) {
		Ensure ensure(ensure_f);
		VALUE result = NULL;
		try {
			result = snow_call(try_f, NULL, 0, NULL);
		}
		catch (snow::Exception ex) {
			if (catch_f)
				return snow_call(catch_f, NULL, 1, &ex.value);
			throw ex;
		}
		return result;
	}

	CAPI void snow_throw_exception(VALUE ex) {
		// TODO: Backtrace and all that jazz
		
		SnObject* str = snow_value_to_string(ex);
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

	void snow_throw_exception_with_description(const char* fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		VALUE ex = snow::string_format_va(fmt, ap);
		va_end(ap);
		snow_throw_exception(ex);
	}
}
