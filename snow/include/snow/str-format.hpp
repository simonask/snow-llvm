#pragma once
#ifndef STR_INTERN_HPP_4AZ8SY7Y
#define STR_INTERN_HPP_4AZ8SY7Y

#include "snow/str.hpp"
#include "snow/snow.hpp"
#include <sstream>
#include <utility>
#include <type_traits>

namespace snow {
	INLINE std::string interpolate_string(const char* format, const ObjectPtr<String>* strings, size_t num_strings) {
		std::stringstream buffer;
		size_t current_arg = 0;
		for (const char* p = format; *p != '\0'; ++p) {
			if (current_arg < num_strings && *p == '%' && *(p+1) == '@') {
				const ObjectPtr<String>& str = strings[current_arg++];
				if (str != nullptr) {
					string_copy_to(str, buffer);
				}
				++p; // skip @
			} else {
				buffer.put(*p);
			}
		}
		return buffer.str();
	}
	
	template <typename T>
	ObjectPtr<String> call_to_string(ObjectPtr<T> val);
	template <>
	INLINE ObjectPtr<String> call_to_string<String>(ObjectPtr<String> str) { return str; }
	template <typename T>
	INLINE ObjectPtr<String> call_to_string(ObjectPtr<T> val) { return value_to_string(val); }
	
	INLINE ObjectPtr<String> call_to_string(Value x) { return value_to_string(x); }
	INLINE ObjectPtr<String> call_to_string(Immediate x) { return value_to_string(x); }
	INLINE ObjectPtr<String> call_to_string(VALUE x) { return value_to_string(x); }
	INLINE ObjectPtr<String> call_to_string(AnyObjectPtr x) { return value_to_string(x); }
	#define SNPRINTF_WRAPPER(TYPE, FORMAT) INLINE ObjectPtr<String> call_to_string(TYPE n) { char buffer[64]; snprintf(buffer, 64, FORMAT, n); return create_string(buffer); }
	SNPRINTF_WRAPPER(int32_t, "%d")
	SNPRINTF_WRAPPER(uint32_t, "%u")
	SNPRINTF_WRAPPER(int64_t, "%lld")
	SNPRINTF_WRAPPER(uint64_t, "%llu")
	SNPRINTF_WRAPPER(size_t, "%lu")
	SNPRINTF_WRAPPER(ssize_t, "%ld")
	SNPRINTF_WRAPPER(float, "%f")
	SNPRINTF_WRAPPER(double, "%lf")
	INLINE ObjectPtr<String> call_to_string(const char* str) { return create_string(str); }
	INLINE ObjectPtr<String> call_to_string(const std::string& str) { return create_string(str.c_str()); }
	
	namespace format {
		template <typename T>
		struct Pointer {
			T val;
		};
		
		template <typename T>
		INLINE Pointer<T> pointer(T val) { Pointer<T> p; p.val = val; return p; }
		
		template <typename T>
		struct Format {
			T val;
			const char* format;
			SN_STATIC_ASSERT(std::is_scalar<T>::value);
		};
		
		template <typename T>
		INLINE Format<T> format(const char* format, T number) { Format<T> p; p.val = number; p.format = format; return p; }
	}
	
	template <typename T>
	INLINE ObjectPtr<String> call_to_string(const format::Pointer<T>& p) {
		char buffer[64];
		snprintf(buffer, 64, "%p", (VALUE)p.val);
		return create_string(buffer);
	}
	
	template <typename T>
	INLINE ObjectPtr<String> call_to_string(const format::Format<T>& p) {
		char buffer[64];
		snprintf(buffer, 64, p.format, p.val);
		return create_string(buffer);
	}
	
	INLINE ObjectPtr<String> call_to_string(byte* ptr) { return call_to_string(format::pointer(ptr)); }
	
	template <typename Target>
	INLINE void convert_arguments_to_string(Target* target) {}
	template <typename Target, typename T>
	INLINE void convert_arguments_to_string(Target* target, T x) {
		*target = call_to_string(x);
	}
	template <typename Target, typename T, typename... Rest>
	INLINE void convert_arguments_to_string(Target* target, T x, Rest... rest) {
		convert_arguments_to_string(target, x);
		convert_arguments_to_string(target+1, rest...);
	}
	
	template <typename... Args>
	INLINE ObjectPtr<String> format_string(const char* format, Args... args) {
		const size_t len = sizeof...(Args);
		ObjectPtr<String> stringified[len];
		convert_arguments_to_string(stringified, args...);
		return create_string(interpolate_string(format, stringified, len).c_str());
	}
	
	template <>
	INLINE ObjectPtr<String> format_string(const char* format) {
		return create_string(format);
	}
}

#endif /* end of include guard: STR_INTERN_HPP_4AZ8SY7Y */
