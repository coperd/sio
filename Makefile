CC=gcc
CFLAGS=-g -I. -lpthread -Wall
LIBS=
INC=

all: sio

sio: sio.c
	$(CC) -o sio sio.c $(CFLAGS)

clean:
	rm -rf *.o sio a.out
