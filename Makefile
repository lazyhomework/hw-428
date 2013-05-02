CC=clang
CFLAGS=	-Wall -Wextra -g -pedantic -pthread -Wno-padded -Weverything -Wno-packed
LDFLAGS= -pthread

all: server client

server:	server.o config.o packets.o routing.o debug.o util.o dht.o packets.h config.h routing.h debug.h dht.h
	$(CC) $(CFLAGS) $(LDFLAGS) server.o config.o packets.o routing.o debug.o util.o dht.o -o server

client: client.o packets.o config.o util.o
	$(CC) $(CFLAGS) $(LDFLAGS) client.o packets.o config.o util.o -o client

util.o: util.h

config.o: config.h

packets.o: packets.h config.h

routing.o: routing.h config.h

debug.o: debug.h

dht.o: dht.h

clean:
	rm -vf *.o server
