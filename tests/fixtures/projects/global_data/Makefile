CRUNCH ?= crunch
SOURCES = main.c message.c state.c
OBJECTS = $(SOURCES:.c=.o)

all: program

program: $(OBJECTS)
	$(CRUNCH) -o $@ $(OBJECTS)

%.o: %.c include/project.h
	$(CRUNCH) -c -I include -o $@ $<
