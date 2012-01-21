LEVEL = ..

MODULES_C_SOURCES = $(wildcard *.c)
MODULES_CXX_SOURCES = $(wildcard *.cpp)
TARGET := $(patsubst %.c,%.snomo,$(MODULES_C_SOURCES)) $(patsubst %.cpp,%.snomo,$(MODULES_CXX_SOURCES))

include $(LEVEL)/Makefile.rules

%.snomo: %.o $(LEVEL)/Makefile.config
	$(LD) $(LDFLAGS) -bundle -lsnow -L$(LEVEL) -o $@ $<
