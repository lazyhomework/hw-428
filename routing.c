#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

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
		printf("To: %ld; next hop: %zd, dist: %u path contains: ", i, table[i].next_hop, table[i].distance);
		for (size_t k = 0; k < MAX_HOSTS; ++k) {
			printf("%d ", table[i].pathentries[k]);
	
		}
		printf("\b\n");
	}
}

pthread_rwlock_t routing_table_lock;

void init_routing_table(node whoami) {
	/* Not really required to lock, but for sanity */
	size_t neighbor;
	struct hostent * temphost;
	
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
		
		temphost = gethostbyname(hosts[neighbor].hostname);
		
		routing_table[neighbor].host = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
		routing_table[neighbor].host->sin_family = temphost->h_addrtype;
		routing_table[neighbor].host->sin_port = htons(hosts[neighbor].routingport);
		
		memcpy(&routing_table[neighbor].host->sin_addr.s_addr, temphost->h_addr_list[0]
		, temphost->h_length);
		
		routing_table[neighbor].data_port = htons(hosts[neighbor].dataport);
	}

	routing_table[whoami].next_hop = whoami;
	routing_table[whoami].distance = 0;
	routing_table[whoami].pathentries[whoami] = true;
	pthread_rwlock_unlock(&routing_table_lock);
}

