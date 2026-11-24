CRUNCH ?= crunch
SOURCES = main.c reader.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/reader.h
	$(CRUNCH) -c -I include -o $@ $<
