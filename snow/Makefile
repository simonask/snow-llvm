LEVEL = .
TARGET := snow

SOURCES := $(wildcard *.c *.cpp)
OBJECTS = $(patsubst %,%.o,$(SOURCES))

SUBDIRS := runtime libsnow lib

include Makefile.rules

$(TARGET): $(SUBDIRS) $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) libsnow/libsnow.a $(LLVM_LIBS) -o $@
