CRUNCH ?= crunch
SOURCES = main.c interact.c greeting.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/io.h
	$(CRUNCH) -c -I include -o $@ $<
