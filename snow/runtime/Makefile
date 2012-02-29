LEVEL = ..
TARGET = objects.txt

SOURCES := $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.cpp.o)
SUBDIRS := x86-64

include $(LEVEL)/Makefile.rules

$(TARGET): $(SUBDIRS) $(OBJECTS)
	echo $(OBJECTS) > $(TARGET)
	cat x86-64/objects.txt | ruby -e 'STDOUT.puts STDIN.read.split(" ").map{|it| "x86-64/"+it}.join(" ")' >> $(TARGET)
