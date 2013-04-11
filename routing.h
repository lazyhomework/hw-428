#ifndef ROUTING_H
#define ROUTING_H

#include <limits.h>
#include <stdbool.h>

#include "config.h"

#define INFINTITY UINT_MAX
#define MAX_TTL 120		//in seconds?
typedef size_t node;

struct route {
	node next_hop;
	unsigned int distance;
	unsigned int ttl;
	bool pathentries[MAX_HOSTS];
};

// index = dest.
struct route routing_table[MAX_HOSTS];
#endif
