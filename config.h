#ifndef CONFIG_H
#define CONFIG_H

#include <string.h>

#define MAX_HOSTS 10

typedef unsigned short port;

struct host {
	char* hostname;
	port dataport;
	port routingport;
	int neighbors[MAX_HOSTS];
};

struct host hosts[MAX_HOSTS];
size_t nhosts;

void printhost(size_t n);
void __attribute__ ((constructor)) initconfig();

extern const int TERMINATOR;
#endif
