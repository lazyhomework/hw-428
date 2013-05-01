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
	X(PACKET_ICMP, "ICMP") \
	X(PACKET_DHT_GET, "dhtget") \
	X(PACKET_DHT_PUT, "dhtput") \
	X(PACKET_MAX, "max")
	
#define ICMP_TYPES \
	Y(ICMP_PING, "ping") \
	Y(ICMP_PING_RESP, "ping response") \
	Y(ICMP_TIMEOUT, "timeout") \
	Y(ICMP_ROUTERR, "routing error") \
	Y(ICMP_MAX, "max")

#define X(a,b) a,
enum packet_type {
	PACKET_TYPES
};
#undef X

#define Y(a,b) a,
enum icmp_type {
	ICMP_TYPES
};
#undef Y

struct __attribute__((packed)) packet {
	enum packet_type magick;
	size_t source;
	size_t dest;
	size_t prevhop;
	size_t ttl;
	size_t datasize;
	port data_port;
	port rout_port;
	void* data;
};

struct __attribute__((packed)) packet_header {
	enum packet_type magick;
	size_t source;
	size_t dest;
	size_t prevhop;
	size_t ttl;
	size_t datasize;
	port data_port;
	port rout_port;
};


struct __attribute__((packed)) icmp_payload {
	enum icmp_type type;
	size_t dest;
	size_t source;
};

void print_pack_h(struct packet_header*);
int fill_buffer(char *buffer, size_t msgsize);
#endif
