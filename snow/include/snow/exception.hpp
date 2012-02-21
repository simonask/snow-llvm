#pragma once
#ifndef EXCEPTION_HPP_M1JVGOQ3
#define EXCEPTION_HPP_M1JVGOQ3

#include "snow/value.hpp"
#include "snow/str-format.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Exception;
	typedef ObjectPtr<Exception> ExceptionPtr;
	typedef ObjectPtr<const Exception> ExceptionConstPtr;
	
	ExceptionPtr create_exception(const std::string& message);
	ExceptionPtr create_exception_with_message(Value message);
	StringPtr exception_get_internal_backtrace(ExceptionConstPtr exception, ssize_t limit = -1);
	StringPtr exception_get_backtrace(ExceptionConstPtr exception, ssize_t limit = -1);
	StringPtr exception_get_source_excerpt(ExceptionConstPtr exception, size_t radius = 3);
	StringPtr exception_get_file(ExceptionConstPtr exception);
	Immediate exception_get_line(ExceptionConstPtr exception);
	Immediate exception_get_column(ExceptionConstPtr exception);
	
	struct Class;
	ObjectPtr<Class> get_exception_class();
	
	void throw_exception(Value exception);
	Value try_catch_ensure(Value try_f, Value catch_f, Value ensure_f);
	
	template <typename... Args>
	void throw_exception_with_description(const char* fmt, Args... args) {
		ObjectPtr<String> description = format_string(fmt, args...);
		throw_exception(description);
	}
}

#endif /* end of include guard: EXCEPTION_HPP_M1JVGOQ3 */
