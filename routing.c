#include "routing.h"

/*
lock the table before calling
*/
void print_routing_table(){
	for(size_t i = 0; i < MAX_HOSTS; ++i){
		printf("To: %d; next hop: %d, dist: %u path contains: ", i, routing_table[i].next_hop, routing_table[i].distance);
		for (size_t k = 0; k < MAX_HOSTS; ++k) {
			printf("%d ", routing_table[i].pathentries[k] ? k : -1);
	
		}
		printf("\b\n");
	}
}

/*
lock the table before calling
*/
void print_rt_ptr(struct route* table){
	for(size_t i = 0; i < MAX_HOSTS; ++i){
		printf("To: %d; next hop: %d, dist: %u path contains: ", i, routing_table[i].next_hop, routing_table[i].distance);
		for (size_t k = 0; k < MAX_HOSTS; ++k) {
			printf("%d ", table[i].pathentries[k] ? k : -1);
	
		}
		printf("\b\n");
	}
}
