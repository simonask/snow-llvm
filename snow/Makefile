LEVEL = .
TARGET := snow

SOURCES = $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.cpp.o)

SUBDIRS := runtime lib tests
SUBDIRS_NOTEST := runtime lib
SUBDIRS_NOLIB := runtime

include $(LEVEL)/Makefile.rules

driver-static: $(SUBDIRS_NOTEST) $(OBJECTS) runtime
	cat runtime/objects.txt | ruby -e 'puts(STDIN.read.split(" ").map{|it|"runtime/"+it}.join("\n"))' | xargs $(LD) $(LDFLAGS) -o $@ $(OBJECTS)

libsnow.$(DYNAMIC_LIBRARY_EXT): $(SUBDIRS_NOLIB)
	cat runtime/objects.txt | ruby -e 'puts(STDIN.read.split(" ").map{|it|"runtime/"+it}.join("\n"))' | xargs $(LD) $(LDFLAGS) -shared -o $@

$(TARGET): lib libsnow.$(DYNAMIC_LIBRARY_EXT) $(OBJECTS)
	$(LD) $(LDFLAGS) -L. -lsnow -o $@ $(OBJECTS)

lib: libsnow.$(DYNAMIC_LIBRARY_EXT)
	$(MAKE) $(CMDGOALS) $(MAKEFLAGS) -C lib

test: $(SUBDIRS)