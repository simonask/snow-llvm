#include "snow/libsnow/libsnow.hpp"
#include <stdio.h>
#include <stdlib.h>

void print_usage(const char* progname) {
	fprintf(stderr, "\tUsage: %s <test-suite.bc>\n", progname);
	exit(1);
}

int main(int argc, char** argv) {
	if (argc < 2) print_usage(argv[0]);
	
	snow::init("../../runtime/snow-runtime.bc");
	int n = snow::run_tests(argv[1]);
	snow::finish();
	return n;
}