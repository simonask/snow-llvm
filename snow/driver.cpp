#include "libsnow/libsnow.hpp"

// TODO: Set up runtime paths etc.

int main (int argc, char const *argv[]) {
	snow::init("runtime/snow-runtime.bc");
	int n = snow::main(argc, argv);
	snow::finish();
	return n;
}