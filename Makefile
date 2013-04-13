CC=clang
CFLAGS=	-Wall -Wextra -g -pedantic -pthread -Wno-padded
LDFLAGS= -pthread

server:	server.o config.o packets.o routing.o debug.o packets.h config.h routing.h debug.h
	$(CC) $(CFLAGS) $(LDFLAGS) server.o config.o packets.o routing.o debug.o -o server

config.o: config.h

packets.o: packets.h config.h

routing.o: routing.h config.h

debug.o: debug.h

clean:
	rm -vf *.o server
