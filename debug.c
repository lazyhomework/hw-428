#include "debug.h"

void print_memblock(void *buf, size_t size, size_t rowwidth){

	unsigned char * print_buf = (unsigned char *)buf;
	
	if(rowwidth == 0) rowwidth = size;
	
	for(size_t i = 0; i < size; ++i){
		if(i%4 == 0){
			printf(" ");
		}else if(i%rowwidth == 0){
			printf("\n");
		}
		printf("%02x",print_buf[i]);
	}
	printf("\n");
}