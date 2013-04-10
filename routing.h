#ifndef ROUTING_H
#define ROUTING_H

#include <stdbool.h>

#define MAX_NODES 10

typedef unsigned int node;

struct route {
	node next_hop;
	unsigned int distance;
	bool pathentries[MAX_NODES];
};
#endif
