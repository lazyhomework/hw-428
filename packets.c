#include "packets.h"

void print_pack_h(struct packet_header* p){
	char * type;

	if (p->magick >= PACKET_MAX) {
		type = "broken";
	}
	else {
#define X(A, B) case A: \
		type = B; \
		break;

		switch(p->magick){
			PACKET_TYPES
		}
#undef X
	}
	
	printf("Type: %s, prev: %zd, source: %zd, dest: %zd, routport: %d, ttl: %zd, size: %zd\n", type,
		p->prevhop, p->source, p->dest, p->rout_port, p->ttl, p->datasize);
}

int fill_buffer(char *buffer, size_t msgsize){
	if(msgsize < sizeof("Hello") + sizeof("Bye")){
		return -1;
	}
	memset(buffer, 'X', msgsize);
	sprintf(buffer, "Hello");
	sprintf(buffer + msgsize -sizeof("Bye"), "Bye");
	buffer[sizeof("Hello")-1] = 'X';

	return 0;
}
