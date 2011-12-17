#include "test.hpp"
#include "snow/fiber.hpp"
#include "snow/function.hpp"
#include "snow/numeric.h"

static VALUE fiber_function(const SnCallFrame* here, VALUE self, VALUE it) {
	printf("fiber started\n");
	ASSERT(snow_type_of(it) == SnFiberType);
	SnObject* cc = (SnObject*)it;
	printf("yielding fiber\n");
	snow_fiber_resume(cc, snow_integer_to_value(1));
	printf("returning fiber\n");
	return snow_integer_to_value(2);
}

BEGIN_TESTS()
BEGIN_GROUP("Basic")

STORY("fiber 1", {
	SnFunction* functor = snow_create_method(fiber_function, snow_sym("fiber"), 1);
	
	printf("launching fiber\n");
	SnObject* fiber = snow_create_fiber(functor);
	VALUE yielded_value = snow_fiber_resume(fiber, NULL);
	printf("fiber yielded\n");
	TEST_EQ(yielded_value, snow_integer_to_value(1));
	printf("resuming fiber\n");
	yielded_value = snow_fiber_resume(fiber, NULL);
	printf("fiber returned\n");
	TEST_EQ(yielded_value, snow_integer_to_value(2));
});

END_GROUP()
END_TESTS()