LEVEL = ../..

SOURCES := $(wildcard test.*.cpp)
TESTS := $(patsubst %.cpp,%.module.bc,$(SOURCES))
TARGET := $(TESTS) test_driver
OBJECTS := test_driver.cpp.o $(patsubst %.cpp,%.cpp.bc,$(SOURCES))
RUN_TESTS := run_tests

include $(LEVEL)/Makefile.rules

test.%.module.bc: test.%.cpp.bc test_module.cpp.bc
	$(BC_LINK) -o $@ $< test_module.cpp.bc

test_driver: test_driver.cpp.o $(LEVEL)/libsnow/libsnow.a $(LLVM_LIBS)
	$(LD) -o $@ test_driver.cpp.o $(LEVEL)/libsnow/libsnow.a $(LLVM_LIBS)

run_tests: test_driver $(TESTS)
	@for t in $(TESTS); do \
		echo "Running $$t:"; \
		./test_driver $$t; \
	done
