CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -pthread

all: dg
spool/sp.o: spool/sp.c
dg: dg.c spool/sp.o

.PHONY:
clean:
	rm -rf spool/sp.o dg
