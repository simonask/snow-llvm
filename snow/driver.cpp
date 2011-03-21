#include "libsnow/libsnow.hpp"
#include <stdlib.h>
#include <stdio.h>
#include "snow/exception.hpp"

// TODO: Set up runtime paths etc.

int main (int argc, char const *argv[]) {
	try {
		snow::init("runtime/snow-runtime.bc");
		int n = snow::main(argc, argv);
		snow::finish();
		return n;
	}
	catch (const snow::Exception& ex) {
		fprintf(stderr, "Caught Snow exception %p\n", ex.value);
		abort();
	}
	catch (...) {
		fprintf(stderr, "Unknown exception caught, aborting!\n");
		abort();
	}
}