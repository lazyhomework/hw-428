CC=clang
CFLAGS=	-Wall -Wextra -g -pedantic -pthread -Wno-padded
LDFLAGS= -pthread

all: server client

server:	server.o config.o packets.o routing.o debug.o util.o packets.h config.h routing.h debug.h
	$(CC) $(CFLAGS) $(LDFLAGS) server.o config.o packets.o routing.o debug.o -o server

client: client.o packets.o config.o util.o

util.o: util.h

config.o: config.h

packets.o: packets.h config.h

routing.o: routing.h config.h

debug.o: debug.h

clean:
	rm -vf *.o server
