CRUNCH ?= crunch
SOURCES = main.c calculate.c custom.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/calculate.h include/custom.h
	$(CRUNCH) -c -I include -o $@ $<
