LEVEL = ..
TARGET = snow-runtime.bc

SOURCES := $(wildcard *.c *.cpp)
OBJECTS = $(patsubst %,%.bc,$(SOURCES))

include $(LEVEL)/Makefile.rules

snow-runtime.bc: $(OBJECTS)
	$(BC_LINK) $(BC_LINKFLAGS) -o $@ $(OBJECTS)
