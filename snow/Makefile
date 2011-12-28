LEVEL = .
TARGET := snow

SOURCES = $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.cpp.o)

SUBDIRS := runtime lib tests
SUBDIRS_NOTEST := runtime lib

include $(LEVEL)/Makefile.rules

$(TARGET): $(SUBDIRS_NOTEST) $(OBJECTS) runtime/libsnow-runtime.a
	$(LD) $(LDFLAGS) $(OBJECTS) runtime/libsnow-runtime.a -o $@

test: $(SUBDIRS)