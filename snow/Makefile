LEVEL = .
TARGET := snow

SOURCES := $(wildcard *.c *.cpp)
OBJECTS := $(patsubst %,%.o,$(SOURCES))

SUBDIRS := runtime lib tests
SUBDIRS_NOTEST := runtime lib

include $(LEVEL)/Makefile.rules

$(TARGET): $(SUBDIRS_NOTEST) $(OBJECTS) runtime/libsnow-runtime.a
	$(LD) $(LDFLAGS) $(OBJECTS) runtime/libsnow-runtime.a -o $@

test: $(SUBDIRS)