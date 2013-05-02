#ifndef CLIENT_H
#define CLIENT_H

#include "routing.h"

enum send_type{
	SEND_DIRECT,
	SEND_PROXY
};
extern node source, dest;

int ping();
long ping_once();
static int getsock(int);
static void init_sockets();
static struct addrinfo getremotehostname(char* hostname, short port);
static void setup(int argc, char* argv[]);
static int client_packet(int sock, enum packet_type type, enum send_type mode, size_t datasize, void *data);
void usage(int err);

#endif
