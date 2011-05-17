LEVEL = ..
TARGET = libsnow-runtime.a

SOURCES := $(wildcard *.c *.cpp) $(wildcard x86-64/*.c x86-64/*.cpp)
OBJECTS = $(patsubst %,%.o,$(SOURCES))

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS)
	-rm $@
	$(AR) rcs $@ $(OBJECTS)
