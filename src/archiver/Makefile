CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

all: crunch-ar crunch-ar.1

crunch-ar: crunch-ar.c ../../include/version.h
	$(CC) $(CFLAGS) -o crunch-ar crunch-ar.c

clean:
	@rm -f crunch-ar
