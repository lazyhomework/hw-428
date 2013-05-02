#ifndef CLIENT_H
#define CLIENT_H

#include "routing.h"

struct ping_ret {
	long long time;
	node reached;
	enum packet_type recved_head;
};

enum send_type{
	SEND_DIRECT,
	SEND_PROXY
};
extern node source, dest;

static int getsock(int);
static void init_sockets();
static int connect_server();
static struct addrinfo getremotehostname(char* hostname, port p);
static void setup(int argc, char* argv[]);
static int client_packet(int sock, enum packet_type type, enum send_type mode, size_t datasize, void *data);
static int client_packet_full(int sock, enum packet_type type, size_t ttl, enum send_type mode, size_t datasize, void *data);
void usage(int err);

#endif
