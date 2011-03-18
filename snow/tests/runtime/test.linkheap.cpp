#include "test.hpp"
#include "snow/runtime/linkheap.hpp"

BEGIN_TESTS()

BEGIN_GROUP("Basic")

STORY("linear allocation", {
	LinkHeap<int> heap;
	for (size_t i = 0; i < 10000; ++i) {
		int* n = heap.alloc();
		TEST_NEQ(n, NULL);
	}
});

END_GROUP()

END_TESTS()