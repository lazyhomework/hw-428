#include <pthread.h>

#include "routing.h"

struct route routing_table[MAX_HOSTS];

/*
lock the table before calling
*/
void print_routing_table(void){
	for(size_t i = 0; i < MAX_HOSTS; ++i){
		printf("To: %zd; next hop: %zd, dist: %u path contains: ", i, routing_table[i].next_hop, routing_table[i].distance);
		for (size_t k = 0; k < MAX_HOSTS; ++k) {
			printf("%d ", routing_table[i].pathentries[k]);
	
		}
		printf("\b\n");
	}
}

/*
lock the table before calling
*/
void print_rt_ptr(struct route* table){
	for(size_t i = 0; i < MAX_HOSTS; ++i){
		printf("To: %ld; next hop: %zd, dist: %zd path contains: ", i, table[i].next_hop, table[i].distance);
		for (size_t k = 0; k < MAX_HOSTS; ++k) {
			printf("%d ", table[i].pathentries[k]);
	
		}
		printf("\b\n");
	}
}

pthread_rwlock_t routing_table_lock;

<<<<<<< HEAD
void init_routing_table(node whoami) {
=======
void init_routing_table(node whoami) {
>>>>>>> 3725e8a2db5594ed5672c57fea9446aeab3ab2d5
	/* Not really required to lock, but for sanity */
	size_t neighbor;
	pthread_rwlock_wrlock(&routing_table_lock);
	
	for (size_t i = 0; i < MAX_HOSTS; ++i) {
		routing_table[i].next_hop = whoami;
		routing_table[i].distance = INFINTITY;
		routing_table[i].ttl = MAX_ROUTE_TTL;
		routing_table[i].host = NULL;
		memset(routing_table[i].pathentries, false , MAX_HOSTS);
	}

	//Connect to our neighbors
	for (size_t i = 0; hosts[whoami].neighbors[i] != TERMINATOR; ++i) {
		neighbor = hosts[whoami].neighbors[i];

		routing_table[neighbor].pathentries[neighbor] = true;
		routing_table[neighbor].pathentries[whoami] = true;
		routing_table[neighbor].next_hop = neighbor;
		routing_table[neighbor].distance = 1;
		//Have to free the host struct at end!
		routing_table[neighbor].host = gethostbyname(hosts[neighbor].hostname);
	}

	routing_table[whoami].next_hop = whoami;
	routing_table[whoami].distance = 0;
	routing_table[whoami].pathentries[whoami] = true;
	pthread_rwlock_unlock(&routing_table_lock);
}

