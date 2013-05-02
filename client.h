#ifndef CLIENT_H
#define CLIENT_H

#include "routing.h"

extern node source, dest;

int ping(int sock);
static int getsock(void);
static struct addrinfo getremotehostname(char* hostname, short port);
static void setup(int argc, char* argv[]);
static int client_packet(int sock, enum packet_type type, size_t datasize, void *data);
void usage(int err);

#endif
