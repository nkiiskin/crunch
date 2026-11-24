CRUNCH ?= crunch
SOURCES = main.c calculate.c adjust.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/calculate.h include/adjust.h
	$(CRUNCH) -c -I include -o $@ $<
