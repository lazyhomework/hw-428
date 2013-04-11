#ifndef PACKETS_H
#define PACKETS_H

#include <string.h>

#define MAX_PACKET 548
#define PACK_HEAD_SIZE sizeof(struct packet) - sizeof(void *)

enum packet_type {
	PACKET_HELLO,
	PACKET_ROUTING
};

struct __attribute__((packed)) packet {
	enum packet_type magick;
	size_t prevhop;
	size_t datasize;
	void* data;
};

struct hello_packet_data {
	char wtf_do_i_need_here;

};




#endif
