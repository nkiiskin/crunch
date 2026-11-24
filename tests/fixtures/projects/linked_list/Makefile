CRUNCH ?= crunch
SOURCES = main.c list.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/list.h
	$(CRUNCH) -c -I include -o $@ $<
