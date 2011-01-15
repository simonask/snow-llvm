LEVEL = .
TARGET := snow

SOURCES := $(wildcard *.c *.cpp)
OBJECTS := $(patsubst %,%.o,$(SOURCES))

SUBDIRS := runtime libsnow lib

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS) $(LLVM_LIBS)
	$(LD) $(LDFLAGS) $(OBJECTS) libsnow/libsnow.a $(LLVM_LIBS) -o $@
