CC=clang
CFLAGS=	-Wall -Wextra -g -pedantic -pthread -Weverything -Wno-padded
LDFLAGS= -pthread

server:	server.o config.o packets.o routing.o packets.h config.h routing.h
	$(CC) $(CFLAGS) $(LDFLAGS) server.o config.o packets.o routing.o -o server

config.o: config.h

packets.o: packets.h config.h

routing.o: routing.h config.h

clean:
	rm -vf *.o server
