CC=clang
CFLAGS=	-Wall -Wextra -std=c99 -pedantic -pthread
LDFLAGS= -pthread

server:	config.o

config.o: config.h

clean:
	rm -vf *.o server
