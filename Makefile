CC=clang
CFLAGS=	-Wall -Wextra -std=c99 -pedantic

server:	config.o

config.o: config.h

clean:
	rm -vf *.o server
