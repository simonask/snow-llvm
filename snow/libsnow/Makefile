LEVEL = ..
TARGET := libsnow.a

SOURCES := $(wildcard *.cpp *.c)
OBJECTS = $(patsubst %,%.o,$(SOURCES))

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS) $(LLVM_LIBS)
	$(AR) rcs $@ $(OBJECTS)

libsnow.$(DYNAMIC_LIBRARY_EXT): libsnow.a
	$(LD) -shared $(LDFLAGS) libsnow.a $(LLVM_LIBS) -o $@