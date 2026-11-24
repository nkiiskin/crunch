CC = gcc
CFLAGS = -Wall -Wextra -O2

all: crunch-ld

crunch-ld: crunch-ld.c relocatable.inc ../../include/version.h
	$(CC) $(CFLAGS) -o crunch-ld crunch-ld.c

crt0.o: crt0.s
	as crt0.s -o crt0.o

clean:
	@rm -f crunch-ld crt0.o

