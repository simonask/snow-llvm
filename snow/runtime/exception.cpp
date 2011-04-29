#include "snow/exception.hpp"
#include "snow/str.h"
#include "snow/snow.h"
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
		fprintf(stderr, "SNOW: Throwing exception %s\n", snow_value_to_cstr(ex));
		throw snow::Exception(ex);
	}

	void snow_throw_exception_with_description(const char* fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		VALUE ex = snow_string_format_va(fmt, ap);
		va_end(ap);
		snow_throw_exception(ex);
	}
}
