#ifndef PACKETS_H
#define PACKETS_H

#include <string.h>
#include <stdio.h>

#include "config.h"

#define MAX_PACKET 548
#define MAX_PACKET_TTL 5

#define PACKET_TYPES \
	X(PACKET_HELLO, "hello") \
	X(PACKET_ROUTING, "routing") \
	X(PACKET_DATA, "data") \
	X(PACKET_CREATE, "create") \
	X(PACKET_TEARDOWN, "teardown") \
	X(PACKET_SENDDATA, "senddata") \
	X(PACKET_MAX, "max") 

#define X(a,b) a,
enum packet_type {
	PACKET_TYPES
};
#undef X

struct __attribute__((packed)) packet {
	enum packet_type magick;
	size_t prevhop;
	size_t dest;
	size_t ttl;
	size_t datasize;
	port data_port;
	port rout_port;
	void* data;
};

struct __attribute__((packed)) packet_header {
	enum packet_type magick;
	size_t prevhop;
	size_t dest;
	size_t ttl;
	size_t datasize;
	port data_port;
	port rout_port;
};

void print_pack_h(struct packet_header*);

#endif
