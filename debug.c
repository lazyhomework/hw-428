#include "debug.h"

int debug_packets;
int debug_routing;
int debug_timing;
int debug_forward;

/*
Print a block of memory
starting from buf, prints hex values in 4 byte groups with
a new line evey rowwidth bytes.
For a single line, use rowwidth = size or 0
*/
void print_memblock(void *buf, size_t size, size_t rowwidth){

	unsigned char * print_buf = (unsigned char *)buf;
	
	if(rowwidth == 0) rowwidth = size;
	
	for(size_t i = 0; i < size; ++i){
		if(i%4 == 0){
			printf(" ");
		}
		if(i%rowwidth == 0){
			printf("\n");
		}
		printf("%02x",print_buf[i]);
	}
	printf("\n");
}

void init_debug(){
	debug_packets = 1;
	debug_timing = 1;
	debug_routing = 1;
	debug_forward = 0;

	//code copied from S.O, allows core dumps.
	// core dumps may be disallowed by parent of this process; change that
	struct rlimit core_limits;
	core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &core_limits);

}
