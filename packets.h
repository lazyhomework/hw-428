#ifndef PACKETS_H
#define PACKETS_H

#include <string.h>

struct packet {
	int magick;
	size_t prevhop;
	void* data;
};

struct hello_packet {
	char wtf_do_i_need_here;

};

#endif
