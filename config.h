#ifndef CONFIG_H
#define CONFIG_H

#include <string.h>

typedef unsigned short port;

struct host {
	char* hostname;
	port dataport;
	port routingport;
	int neighbors[100];
};

struct host hosts[10];
size_t nhosts;
#endif
