#include "packets.h"

void print_pack_h(struct packet_header* p){
	char * type;

	if (p->magick >= PACKET_MAX) {
		type = "broken";
	}
	else {
		switch(p->magick){
			case PACKET_ROUTING :
				type = "routing";
				break;
			case PACKET_HELLO :
				type = "hello";
				break;
			case PACKET_DATA :
				type = "data";
				break;
			case PACKET_MAX :
				type = "broken";
				break;
		}
	}
	
	printf("Type: %s, prev: %zd, dest: %zd, routport: %d, ttl: %zd, size: %zd\n", type, p->prevhop,
		p->dest, p->rout_port, p->ttl, p->datasize);
}
