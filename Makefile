CC=clang
CFLAGS=	-Wall -Wextra -std=c99 -pedantic -pthread
LDFLAGS= -pthread

server:	server.o config.o packets.o routing.o
	$(CC) $(CFLAGS) $(LDFLAGS) server.o config.o packets.o routing.o -o server

config.o: config.h

packets.o: packets.h config.h

routing.o: routing.h config.h

clean:
	rm -vf *.o server
