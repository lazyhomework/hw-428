#ifndef PACKETS_H
#define PACKETS_H

#include <string.h>

enum packet_type {
	PACKET_HELLO
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
