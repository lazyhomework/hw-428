#ifndef PACKETS_H
#define PACKETS_H

#include <string.h>
#include <stdio.h>

#define MAX_PACKET 548
#define MAX_PACKET_TTL 5
enum packet_type {
	PACKET_HELLO,
	PACKET_ROUTING,
	PACKET_MAX
};

struct __attribute__((packed)) packet {
	enum packet_type magick;
	size_t prevhop;
	size_t dest;
	size_t ttl;
	size_t datasize;
	void* data;
};

struct __attribute__((packed)) packet_header {
	enum packet_type magick;
	size_t prevhop;
	size_t dest;
	size_t ttl;
	size_t datasize;
};

struct hello_packet_data {
	char wtf_do_i_need_here;

};

void print_pack_h(struct packet_header*);



#endif
