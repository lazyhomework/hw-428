#ifndef ROUTING_H
#define ROUTING_H

#include <limits.h>
#include <stdbool.h>
#include <netdb.h>
#include <stdio.h>

#include "config.h"
#include "packets.h"
#include "debug.h"
#include "util.h"

#define OPTION_ROUTE 0
#define OPTION_DATA 1

#define EFORWARD -2

#define INFINTITY (UINT_MAX -1)
#define MAX_ROUTE_TTL 40		//in seconds

typedef size_t node;

/*
The data port field is stored in machine form,
and the port in *host is in network form (its the routing port)
*/
struct route {
	node next_hop;
	struct sockaddr_in *host;
	port data_port;
	unsigned int distance;
	int ttl;
	bool pathentries[MAX_HOSTS];
};

// index = dest.
extern struct route routing_table[MAX_HOSTS];

void print_routing_table(void);
void print_rt_ptr(struct route*);


extern pthread_rwlock_t routing_table_lock;
void init_routing_table(node whoami);
int add_neighbor(node whoami, size_t neighbor);

int send_packet_ttl(int sock, enum packet_type type, size_t ttl, node dest, node source, size_t datasize, void *data, int option);
int forward_packet(unsigned char *buffer, int sock, node whoami, int option);
void remove_entry(node whoami, node neighbor);
int send_packet(int sock, enum packet_type type, node source, node dest, size_t datasize, void *data, int option);
#endif
