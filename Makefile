CC=gcc
CFLAGS=-g -I. -lpthread -Wall
LIBS=
INC=

all: sio

sio: sio.o thread.o util.o
	$(CC) -o sio sio.o thread.o util.o $(CFLAGS)

sio.o: sio.c sio.h
	$(CC) -c sio.c 

thread.o: thread.c thread.h
	$(CC) -c thread.c

util.o: util.c util.h
	$(CC) -c util.c

.PHONY clean:
	rm -rf *.o sio a.out
