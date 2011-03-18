LEVEL = .
TARGET := snow

SOURCES := $(wildcard *.c *.cpp)
OBJECTS := $(patsubst %,%.o,$(SOURCES))

SUBDIRS := runtime libsnow lib tests

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS) $(LLVM_LIBS) libsnow/libsnow.a
	$(LD) $(LDFLAGS) $(OBJECTS) libsnow/libsnow.a $(LLVM_LIBS) -o $@
