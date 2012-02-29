LEVEL := ../..
TARGET := objects.txt

SOURCES := $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.cpp.o)

include $(LEVEL)/Makefile.rules

$(TARGET): $(OBJECTS)
	echo $(OBJECTS) > $(TARGET)