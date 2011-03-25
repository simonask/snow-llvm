LEVEL = .
TARGET := snow

SOURCES := $(wildcard *.c *.cpp)
OBJECTS := $(patsubst %,%.o,$(SOURCES))

SUBDIRS := runtime libsnow lib tests
SUBDIRS_NOTEST := runtime libsnow lib

include $(LEVEL)/Makefile.rules

$(TARGET): $(SUBDIRS_NOTEST) $(OBJECTS) $(LLVM_LIBS) libsnow/libsnow.a
	$(LD) $(LDFLAGS) $(OBJECTS) libsnow/libsnow.a $(LLVM_LIBS) -o $@

test: $(SUBDIRS)