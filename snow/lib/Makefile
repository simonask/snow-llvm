LEVEL = ..

MODULES_C_SOURCES = $(wildcard *.c)
MODULES_CXX_SOURCES = $(wildcard *.cpp)
TARGET := $(patsubst %.c,%.bc,$(MODULES_C_SOURCES)) $(patsubst %.cpp,%.bc,$(MODULES_CXX_SOURCES))

include $(LEVEL)/Makefile.rules
