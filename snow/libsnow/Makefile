LEVEL = ..
TARGET := libsnow.a

SOURCES := $(wildcard *.cpp *.c) $(wildcard x86-64/*.c)
LLVM_BACKEND_SOURCES = $(wildcard llvm-backend/*.cpp)
OBJECTS := $(patsubst %,%.o,$(SOURCES)) $(patsubst %,%.o,$(LLVM_BACKEND_SOURCES))
INCLUDES := -I.

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS) $(LLVM_LIBS)
	$(AR) rcs $@ $(OBJECTS)

libsnow.$(DYNAMIC_LIBRARY_EXT): libsnow.a
	$(LD) -shared $(LDFLAGS) libsnow.a $(LLVM_LIBS) -o $@