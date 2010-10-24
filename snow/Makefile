LEVEL = .
TARGET := snow

SOURCES := $(wildcard *.c *.cpp)
OBJECTS = $(patsubst %,%.o,$(SOURCES))

SUBDIRS := runtime libsnow

include Makefile.rules

snow: libsnow runtime $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) libsnow/libsnow.a $(LLVM_LIBS) -o $@
