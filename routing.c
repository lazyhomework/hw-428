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
		printf("To: %zd; next hop: %zd, dist: %u, TTL: %u path contains: ", i, routing_table[i].next_hop, 
			routing_table[i].distance, routing_table[i].ttl);
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
		printf("To: %ld; next hop: %zd, dist: %u, TTL: %u path contains: ", i, table[i].next_hop, table[i].distance, table[i].ttl);
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
		
		add_neighbor(whoami,neighbor);
	}

	routing_table[whoami].next_hop = whoami;
	routing_table[whoami].distance = 0;
	routing_table[whoami].pathentries[whoami] = true;
	pthread_rwlock_unlock(&routing_table_lock);
}

/*Lock table before calling*/
void add_neighbor(node whoami, size_t neighbor){
		struct hostent * temphost;
		
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

/*
Returns -2 if can't get to dest, -1 if sendto fails, 0 otherwise
Do not call from function that currently has routing table lock (this function locks it)
*/
int send_packet(int sock, enum packet_type type, node dest, node source, size_t datasize, void *data, int option){
	int err;
	struct sockaddr_in addr;
	struct packet_header header;
	
	unsigned char * buffer = malloc(datasize + sizeof(struct packet_header));
	
	header.magick = type;
	header.dest = dest;
	header.rout_port = hosts[source].routingport;
	header.data_port = hosts[source].dataport;
	header.ttl = MAX_PACKET_TTL;
	header.prevhop = source;
	header.datasize = datasize;
	memcpy(buffer,&header,sizeof(struct packet_header));
	memcpy(buffer + sizeof(struct packet_header),data, datasize);
	
	pthread_rwlock_rdlock(&routing_table_lock);
	
	if(routing_table[dest].distance < INFINTITY){
		node next_hop = routing_table[dest].next_hop;
	
		addr.sin_family = routing_table[next_hop].host->sin_family;
		addr.sin_addr.s_addr = routing_table[next_hop].host->sin_addr.s_addr;
		switch(option){
			case OPTION_DATA:
				addr.sin_port = htons(routing_table[next_hop].data_port);
				break;
			case OPTION_ROUTE:
				addr.sin_port = htons(routing_table[next_hop].host->sin_port);
				break;
		}
	}else{
		free(buffer);
		return -2;
	}
	pthread_rwlock_unlock(&routing_table_lock);
	
	size_t packet_size = sizeof(struct packet_header) + datasize;
	err = sendto(sock, buffer, packet_size, 0, (struct sockaddr *) &addr, sizeof(addr));
	if(err < 0){
		perror("sendto: ");
		free(buffer);
		return -1;
	}
	
	free(buffer);
	return 0;
}
