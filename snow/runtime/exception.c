#include "snow/exception.h"
#include <stdio.h>
#include <stdlib.h>

void snow_throw_exception(VALUE ex) {
	// TODO!!! Implement exceptions
	TRAP(); // exception
}

void snow_throw_exception_with_description(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char* text;
	vasprintf(&text, fmt, ap);
	va_end(ap);
	fprintf(stderr, "EXCEPTION: %s\n", text);
	free(text);
	exit(1);
}