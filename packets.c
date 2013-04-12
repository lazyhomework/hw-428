#include "packets.h"

void print_pack_h(struct packet_header* p){
	char * type;
	
	switch(p->magick){
		case PACKET_ROUTING :
			type = "routing";
			break;
		case PACKET_HELLO :
			type = "hello";
			break;
		default:
			type = "broken";
			break;
	}
	
	printf("Type: %s, prev: %d, dest: %d, ttl: %d, size: %d\b\n", type, p->prevhop,
		p->dest, p->ttl, p->datasize);
}
