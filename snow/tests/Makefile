LEVEL = ..

SOURCES := $(wildcard test.*.cpp)
TARGET := $(patsubst %.cpp,%,$(SOURCES))
RUN_TESTS := run_tests

test.%: test.%.cpp.o test.cpp.o
	$(LD) -o $@ $< test.cpp.o

NUM_TESTS = `echo "$(TARGET)" | wc -w`

run_tests: $(TARGET)
	@echo Running $(NUM_TESTS) test suite\(s\).
	@for t in $(TARGET); do \
		echo "Running $$t:"; \
		./$$t; \
	done

SUBDIRS = runtime

include $(LEVEL)/Makefile.rules
