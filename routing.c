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
		printf("To: %zd; next hop: %zd, dist: %u, TTL: %u path contains: ", i, table[i].next_hop, table[i].distance, table[i].ttl);
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
int add_neighbor(node whoami, size_t neighbor){
		struct hostent * temphost;
		
		if(routing_table[neighbor].host != NULL){
			return -1;
		}
		memset(routing_table[neighbor].pathentries, 0,MAX_HOSTS*sizeof(bool));
		routing_table[neighbor].pathentries[neighbor] = true;
		routing_table[neighbor].pathentries[whoami] = true;
		routing_table[neighbor].next_hop = neighbor;
		routing_table[neighbor].distance = 1;
		//Have to free the host struct at end!
		
		temphost = gethostbyname(hosts[neighbor].hostname);
		
		if(temphost == NULL){
			return -1;
		}
		routing_table[neighbor].host = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
		routing_table[neighbor].host->sin_family = temphost->h_addrtype;
		routing_table[neighbor].host->sin_port = htons(hosts[neighbor].routingport);
		
		memcpy(&routing_table[neighbor].host->sin_addr.s_addr, temphost->h_addr_list[0]
		, temphost->h_length);
		
		routing_table[neighbor].data_port = hosts[neighbor].dataport;
		return 0;
}

/*write lock table before call*/
void remove_entry(node whoami, node neighbor){

	routing_table[neighbor].next_hop = whoami;
	routing_table[neighbor].distance = INFINTITY;
	routing_table[neighbor].ttl = MAX_ROUTE_TTL;
	memset(routing_table[neighbor].pathentries, false , MAX_HOSTS);	

	if(routing_table[neighbor].host != NULL){
		free(routing_table[neighbor].host);
		routing_table[neighbor].host = NULL;
	}

}
/*
Returns -2 if can't get to dest, -1 if sendto fails, 0 otherwise
Do not call from function that currently has routing table lock (this function locks it)
*/
static int send_packet_full(int sock, enum packet_type type, size_t ttl, node source, node dest, node prevhop, size_t datasize, void *data, int option){
	int err;
	struct sockaddr_in addr;
	struct packet_header header;
	
	unsigned char * buffer = malloc(datasize + sizeof(struct packet_header));
	
	header.magick = type;
	header.source = source;
	header.dest = dest;
	header.prevhop = prevhop;
	header.rout_port = hosts[prevhop].routingport;
	header.data_port = hosts[prevhop].dataport;
	header.ttl = ttl;

	header.datasize = datasize;
	memcpy(buffer,&header,sizeof(struct packet_header));
	memcpy(buffer + sizeof(struct packet_header),data, datasize);

#ifdef ROUTING_DEBUG
	printf("send: ");
	print_pack_h(&header);
#endif
	pthread_rwlock_rdlock(&routing_table_lock);
	
	if(routing_table[dest].distance < INFINTITY ){
		node next_hop = routing_table[dest].next_hop;
		
		if(routing_table[next_hop].host == NULL){
			goto iforgottochecknullbefore;
		}
		addr.sin_family = routing_table[next_hop].host->sin_family;
		addr.sin_addr.s_addr = routing_table[next_hop].host->sin_addr.s_addr;
		switch(option){
		case OPTION_DATA:
			addr.sin_port = htons(routing_table[next_hop].data_port);
			break;
		case OPTION_ROUTE:
			addr.sin_port = routing_table[next_hop].host->sin_port;
			break;
		}
	}else{
	
iforgottochecknullbefore:	
		free(buffer);
		pthread_rwlock_unlock(&routing_table_lock);
		return EFORWARD;
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

/*overloaded for auto max ttl and source = prevhop*/
int send_packet(int sock, enum packet_type type, node source, node dest, size_t datasize, void *data, int option){
	return send_packet_full(sock, type, MAX_PACKET_TTL, source, dest, source, datasize, data, option);
}

/*
Option is 0 to send to routing port, 1 for data.
TODO make option an enum
Buffer must have a packet_header at the start of it, with accurate datasize field.
Decrements TTL field in supplied header
By modifying the header in place, this method is faster than send_packet().
DO NOT CALL FROM FUNCTION WITH ROUTING LOCK

Returns:
0 on succes
ETIMEOUT if ttl in header <= 1;
EFORWARD if dest in header unreachable
ENOSEND if sendto fails for any reason. errno should be set to sendto failure.
*/
int forward_packet(unsigned char *buffer, int sock, node whoami, int option){
	struct sockaddr_in addr;
	struct packet_header* out_header = (struct packet_header*) buffer;
	
	int err = 0;
	
	//Update ttl, drop if 0
	if((out_header->ttl -= 1) <= 0){
		//drop packet
		return ETIMEOUT;
	}
	
	out_header->prevhop = whoami;
	out_header->data_port = hosts[whoami].dataport;
	out_header->rout_port = hosts[whoami].routingport;
	
	pthread_rwlock_rdlock(&routing_table_lock);

	if(routing_table[out_header->dest].distance < INFINTITY){
		node next_hop = routing_table[out_header->dest].next_hop;
		
		if(routing_table[next_hop].host == NULL){
			pthread_rwlock_unlock(&routing_table_lock);
			return EFORWARD;
		}
		
		addr.sin_family = routing_table[next_hop].host->sin_family;
		addr.sin_addr.s_addr = routing_table[next_hop].host->sin_addr.s_addr;
		switch(option){
		case OPTION_DATA:
			addr.sin_port = htons(routing_table[next_hop].data_port);
			break;
		case OPTION_ROUTE:
			addr.sin_port = routing_table[next_hop].host->sin_port;
			break;
		}
	}else{
		//routing error, we can't forward this packet. Drop it
		pthread_rwlock_unlock(&routing_table_lock);
		return EFORWARD;
	}
	
	pthread_rwlock_unlock(&routing_table_lock);
	
	size_t packet_size = sizeof(struct packet_header) + out_header->datasize;
	err = sendto(sock, buffer, packet_size, 0, (struct sockaddr *) &addr, sizeof(addr));
	if(err < 0){
		return ENOSEND;
	}
	
	return 0;
}

int fwdto_client(unsigned char *buffer, int sock, node whoami, struct client_info client_addr, int option){

	struct sockaddr_in addr;
	struct packet_header* out_header = (struct packet_header*) buffer;
	
	int err = 0;
	
	out_header->ttl = MAX_PACKET_TTL;
	
	out_header->prevhop = whoami;
	out_header->data_port = hosts[whoami].dataport;
	out_header->rout_port = hosts[whoami].routingport;
	out_header->dest = CLIENT_NODE;
		
	addr.sin_family = client_addr.addr.sin_family;
	addr.sin_addr.s_addr = client_addr.addr.sin_addr.s_addr;
	switch(option){
	case OPTION_DATA:
		addr.sin_port = client_addr.data_port;
		break;
	case OPTION_ROUTE:
		addr.sin_port = client_addr.route_port;
		break;
	}
	
	size_t packet_size = sizeof(struct packet_header) + out_header->datasize;
	err = sendto(sock, buffer, packet_size, 0, (struct sockaddr *) &addr, sizeof(addr));
	if(err < 0){
		return ENOSEND;
	}
	
	return 0;
}
