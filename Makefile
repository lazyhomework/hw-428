CC=clang
CFLAGS=	-Wall -Wextra -std=c99 -pedantic -pthread
LDFLAGS= -pthread

server:	server.o config.o packets.o

config.o: config.h

packets.o: packets.h

clean:
	rm -vf *.o server
