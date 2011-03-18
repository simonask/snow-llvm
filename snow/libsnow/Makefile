LEVEL = ..
TARGET := libsnow.a

SOURCES := $(wildcard *.cpp *.c) $(wildcard llvm-backend/*.cpp llvm-backend/*.c x86-64/*.c)
OBJECTS := $(patsubst %,%.o,$(SOURCES))
INCLUDES := -I.

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS) $(LLVM_LIBS)
	$(AR) rcs $@ $(OBJECTS)

libsnow.$(DYNAMIC_LIBRARY_EXT): libsnow.a
	$(LD) -shared $(LDFLAGS) libsnow.a $(LLVM_LIBS) -o $@