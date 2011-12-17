#include "snow/snow.hpp"
#include <stdlib.h>
#include <stdio.h>
#include "snow/exception.hpp"

// TODO: Set up runtime paths etc.

int main (int argc, char* const* argv) {
	try {
		snow_init("lib/prelude.sn");
		int n = snow_main(argc, argv);
		snow_finish();
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