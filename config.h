#ifndef CONFIG_H
#define CONFIG_H

#include <string.h>

#define MAX_HOSTS 6
#define CLIENT_NODE 99

typedef unsigned short port;

struct host {
	char* hostname;
	port dataport;
	port routingport;
	int neighbors[MAX_HOSTS+1];
};

extern struct host hosts[MAX_HOSTS];
extern const int TERMINATOR;

void printhost(size_t n);
void __attribute__ ((constructor)) initconfig();

#endif
