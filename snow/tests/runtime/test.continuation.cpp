#include "test.hpp"
#include "snow/continuation.h"
#include "snow/function.h"
#include "snow/numeric.h"
#include "snow/type.h"

static VALUE continuation_function(SnFunctionCallContext* here, VALUE self, VALUE it) {
	printf("fiber started\n");
	ASSERT(snow_type_of(it) == SnContinuationType);
	SnContinuation* cc = (SnContinuation*)it;
	printf("yielding fiber\n");
	snow_continuation_resume(cc, snow_integer_to_value(1));
	printf("returning fiber\n");
	return snow_integer_to_value(2);
}

BEGIN_TESTS()
BEGIN_GROUP("Basic")

STORY("fiber 1", {
	SnFunction* functor = snow_create_method(continuation_function, snow_sym("fiber"), 1);
	
	printf("launching fiber\n");
	SnContinuation* fiber = snow_continuation_callcc(functor);
	printf("fiber yielded\n");
	TEST_EQ(snow_continuation_get_returned_value(fiber), snow_integer_to_value(1));
	printf("resuming fiber\n");
	snow_continuation_resume(fiber, NULL);
	printf("fiber returned\n");
	TEST_EQ(snow_continuation_get_returned_value(fiber), snow_integer_to_value(2));
});

END_GROUP()
END_TESTS()