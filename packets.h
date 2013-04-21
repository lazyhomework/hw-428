#ifndef PACKETS_H
#define PACKETS_H

#include <string.h>
#include <stdio.h>

#include "config.h"

#define MAX_PACKET 548
#define MAX_PACKET_TTL 5
enum packet_type {
	PACKET_HELLO,
	PACKET_ROUTING,
	PACKET_DATA,
	PACKET_CREATE,
	PACKET_TEARDOWN,
	PACKET_MAX
};

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
