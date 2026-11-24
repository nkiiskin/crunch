CRUNCH ?= crunch
SOURCES = main.c values.c update.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/shared.h
	$(CRUNCH) -c -I include -o $@ $<
