CRUNCH ?= crunch
SOURCES = main.c data.c resize.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/data.h
	$(CRUNCH) -c -I include -o $@ $<
