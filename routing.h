#ifndef ROUTING_H
#define ROUTING_H

#include <limits.h>
#include <stdbool.h>
#include <netdb.h>
#include <stdio.h>

#include "config.h"

#define INFINTITY UINT_MAX
#define MAX_ROUTE_TTL 120		//in seconds?
typedef size_t node;

struct route {
	node next_hop;
	struct hostent *host;
	unsigned int distance;
	int ttl;
	bool pathentries[MAX_HOSTS];
};

// index = dest.
struct route routing_table[MAX_HOSTS];

void print_routing_table(int whoami);
void print_rt_ptr(struct route*);


extern pthread_rwlock_t routing_table_lock;
void init_routing_table(int whoami);
#endif
