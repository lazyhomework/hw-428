#ifndef ROUTING_H
#define ROUTING_H

#include <stdbool.h>

#define MAX_NODES 10

typedef size_t node;

struct route {
	node next_hop;
	unsigned int distance;
	bool pathentries[MAX_NODES];
};
#endif
