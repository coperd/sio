CC=gcc
CFLAGS=-g -I. -lpthread -Wall -Wunused-variable
LIBS=
INC=

all: sio

sio: sio.o thread.o util.o
	@$(CC) -o sio sio.o thread.o util.o $(CFLAGS)
	@echo "CC $<"

sio.o: sio.c sio.h
	@$(CC) -c sio.c 
	@echo "CC $<"

thread.o: thread.c thread.h
	@$(CC) -c thread.c
	@echo "CC $<"

util.o: util.c util.h
	@$(CC) -c util.c
	@echo "CC $<"

.PHONY clean:
	rm -rf *.o sio a.out
