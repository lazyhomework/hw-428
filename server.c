#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "config.h"
#include "packets.h"
#include "routing.h"
#include "debug.h"
#include "util.h"


static node whoami;
static port this_dataport;
static port this_routingport;
/*
TODO list
free host struct in hosts
fix unlocked table access in timer thread. tablecpy.host is ptr to mem that should be locked. soln: copy addr to stack.
Clear up host vs network on ports
*/

volatile sig_atomic_t continue_running;

static void hdl(int sig, siginfo_t *siginfo, void* context) {
	continue_running = 0;
}

static void usage(int err) {
	printf("./server -n nodeid\n");
	exit (err);
}

static void setup(int argc, char* argv[]) {
	char ch;

	int required = 0x0;

	while (((ch = getopt(argc, argv, "hn:")) != -1)) {
		switch (ch) {
			case 'h':
				usage(0);
				break;
			case 'n':
				required |= 0x1;
				whoami = atoi(optarg);
				if(whoami > MAX_HOSTS-1){
					die("Node out of range",-1);
				}
				break;
			case '?':
			default:
				usage(1);
		}
	}
	if (required < 0x1) {
		usage(2);
	}
	
	this_dataport = hosts[whoami].dataport;
	this_routingport = hosts[whoami].routingport;

}

enum thread_types {
	THREAD_ROUTING,
	THREAD_TIMER,
	THREAD_FORWARD,
	THREAD_MAX
};


static void* timerthread(void* data){
	unsigned int interval = 10; //in sec
	int sock = *((int*) data);
	int buffersize = MAX_HOSTS * sizeof(struct route) + sizeof(struct packet_header);
	int err, neighbor;
	
	struct sockaddr_in targetaddr;
	struct packet_header header = ((struct packet_header) {.magick=PACKET_ROUTING, .prevhop=whoami, .dest = 0, .ttl=MAX_PACKET_TTL, 
		.datasize=buffersize, .rout_port = hosts[whoami].routingport});
	
	unsigned char buffer[buffersize];

	//tablecpy points to the buffer, starting immediately after the packet header.
	struct route* tablecpy = (struct route *)(buffer + sizeof(struct packet_header));

		
	while(continue_running){
		sleep(interval);
		
		pthread_rwlock_wrlock(&routing_table_lock);

		//update the ttl on all routes		
		for (size_t i = 0; i < MAX_HOSTS; ++i) {
			//Don't decrement the ttl on table entry for self
			if(i != whoami && (routing_table[i].ttl -= interval) <= 0){
				routing_table[i].next_hop = whoami;
				routing_table[i].distance = INFINTITY;
				routing_table[i].ttl = MAX_ROUTE_TTL;
				free(routing_table[i].host);
				routing_table[i].host = NULL;
				
				memset(routing_table[i].pathentries, false , MAX_HOSTS);
			}
		}
		//copy the routing table to buffer.
		memcpy(tablecpy,routing_table, MAX_HOSTS * sizeof(struct route));
		
		pthread_rwlock_unlock(&routing_table_lock);
		
#ifdef TIMING_DEBUG
		printf("Table after update ttl \n");
		print_rt_ptr(tablecpy);
#endif
		
		
		header.rout_port = this_routingport;
		header.data_port = this_dataport;
		
		//Go through list of neighbors, send the table to them.
		for (size_t i = 0; i < MAX_HOSTS; ++i) {			
			if(tablecpy[i].distance == 1 && tablecpy[i].host != NULL){
			
				neighbor = i;
			
				header.dest = neighbor;
				memcpy(buffer,&header,sizeof(struct packet_header));

				err = sendto(sock, buffer, buffersize, 0, (struct sockaddr *) tablecpy[i].host, sizeof(targetaddr));
				if(err < 0){
					die("Timer send",errno);
				}
			}
		}
	}
	return NULL;
}

static void* routingthread(void* data) {
	int sock = *((int*) data);
	int err;

	struct sockaddr_in addr;
	struct packet_header header;
	struct route * path;
	
	unsigned char rcvbuf[MAX_PACKET] = { 0 };
	socklen_t addrsize = sizeof(addr);

	if (rcvbuf == NULL) {
		die("malloc", errno);
	}
	

	while(continue_running){
	
		//non-blocking read, peeks at data
		err = recvfrom(sock, rcvbuf, sizeof(struct packet_header), MSG_DONTWAIT|MSG_PEEK,(struct sockaddr *)&addr, &addrsize);
		if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}

		if(err < 0){
			die("Receive from", errno);
		}else if(err < sizeof(struct packet_header)){
			die("short read", err);
		}
	
		memcpy(&header, rcvbuf, sizeof(struct packet_header));
#ifdef ROUTING_DEBUG
		print_pack_h(&header);
		//printf("Raw buffer\n");
		//print_memblock(rcvbuf, sizeof(struct packet_header), 0);
#endif	
		if(header.magick == PACKET_HELLO){
			//not needed with dist vector routing
			die("Sould not receive hello",-1);
			
		}else if(header.magick == PACKET_ROUTING){
			err = recvfrom(sock, rcvbuf, sizeof(struct packet_header) + header.datasize, MSG_DONTWAIT, (struct sockaddr *) &addr, &addrsize);
			if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
				continue;
			}
			if(err < 0){
				die("Receive from", errno);
			}else if(err < header.datasize){
				die("short read", err);
			}
				
			pthread_rwlock_wrlock(&routing_table_lock);
			
			path = (struct route *) (rcvbuf+sizeof(struct packet_header));
			
#ifdef ROUTING_DEBUG
			//printf("Raw buffer\n");
			//print_memblock(rcvbuf,sizeof(struct route)*MAX_HOSTS, sizeof(struct route));
			
			printf("Old routing table\n");				
			print_routing_table();
			printf("Table reveived from %lu\n", header.prevhop);
			print_rt_ptr(path);
#endif
			//If we dont know how to talk to who sent us a message, add it.
			if(routing_table[header.prevhop].host == NULL){
				routing_table[header.prevhop].host = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
			}
			//Update our info on how to contact sender on every routing packet
			routing_table[header.prevhop].host->sin_family = addr.sin_family;
			routing_table[header.prevhop].host->sin_port = htons(header.rout_port);
			routing_table[header.prevhop].host->sin_addr.s_addr = addr.sin_addr.s_addr;

			routing_table[header.prevhop].data_port = header.data_port;
			
			for(size_t i = 0; i < MAX_HOSTS; ++i){				
				//we're not part of path AND ( Distance is shorter OR table came from next hop on path )
				if( !path[i].pathentries[whoami] 
						&& (path[i].distance +1 < routing_table[i].distance
						|| routing_table[i].next_hop == header.prevhop ) ){

					routing_table[i].ttl = MAX_ROUTE_TTL;
					memcpy(routing_table[i].pathentries, path[i].pathentries, MAX_HOSTS);
					
					//Handle the case where next hop lost its link
					if(path[i].distance == INFINTITY){
						routing_table[i].distance = INFINTITY;
						routing_table[i].next_hop = whoami;
					}else{	
						routing_table[i].distance = path[i].distance + 1;
						routing_table[i].next_hop = header.prevhop;
						routing_table[i].pathentries[whoami] = true;
					}
					
				//Refresh the ttl on paths through sender
				}else if(routing_table[i].next_hop == header.prevhop ){
					routing_table[i].ttl = MAX_ROUTE_TTL;
				}
			}

#ifdef ROUTING_DEBUG
			printf("new routing table updated from host #%ld\n", header.prevhop);
			print_routing_table();
#endif		
			pthread_rwlock_unlock(&routing_table_lock);
		}else{
			die("Malformed headerid",-1);
		}



	}	
	return NULL;
}

#define FORWARD_ROUT 0
#define FORWARD_DATA 1
/*
Option is 0 to send to routing port, 1 for data.
TODO make this an enum
*/
int forward_packet(unsigned char *buffer, int sock, int option){
	struct sockaddr_in addr;
	struct packet_header* out_header = (struct packet_header*) buffer;
	
	int err = 0;
	
	//Update ttl, drop if 0
	if((out_header->ttl -= 1) <= 0){
		//drop packet
#ifdef FORWARD_DEBUG
		printf("Dropped packet for ttl timeout.\n");
		
#endif
		return -1;
	}
	
	out_header->prevhop = whoami;
	out_header->data_port = htons(this_dataport);
	out_header->rout_port = htons(this_routingport);
	
	pthread_rwlock_rdlock(&routing_table_lock);


	if(routing_table[out_header->dest].distance < INFINTITY){
		node next_hop = routing_table[out_header->dest].next_hop;
		
		addr.sin_family = routing_table[next_hop].host->sin_family;
		addr.sin_addr.s_addr = routing_table[next_hop].host->sin_addr.s_addr;
		switch(option){
			case FORWARD_DATA:
				addr.sin_port = htons(routing_table[next_hop].data_port);
				break;
			case FORWARD_ROUT:
				addr.sin_port = htons(routing_table[next_hop].host->sin_port);
				break;
		}
	}else{
		//routing error, we can't forward this packet. Drop it
#ifdef FORWARD_DEBUG
		printf("Dropped packet for routing error, can't forward. Dest: %zu\n", out_header->dest);
#endif				
	}
	
	pthread_rwlock_unlock(&routing_table_lock);
	
	size_t packet_size = sizeof(struct packet_header) + out_header->datasize;
	err = sendto(sock, buffer, packet_size, 0, (struct sockaddr *) &addr, sizeof(addr));
	if(err < 0){
		die("Forward packet send",errno);
	}
	
	return 0;
}
/*
Currently has no buffer, forwards as fast a possible.
Buffer is limited by underlying socket
*/
static void* forwardingthread(void *data){
	int sock = *((int*)data);
	int err = 0;
	node next_hop;
	size_t packet_size = 0;
	
	unsigned char rcvbuf[MAX_PACKET] = { 0 };

	struct sockaddr_in addr;
	struct packet_header input_header;
	struct packet_header* out_header = (struct packet_header*) rcvbuf;
	
	while(continue_running){
	
		//non-blocking read, peeks at data
		err = recvfrom(sock, &input_header, sizeof(struct packet_header), MSG_DONTWAIT|MSG_PEEK, NULL, 0);
		if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		if(err < 0){
			die("Receive from", errno);
		}else if(err < sizeof(struct packet_header)){
			die("short read", err);
		}
		
		//Possibly add other error checking here
		if(input_header.magick == PACKET_DATA){
			
			//non-blocking read, consumes data
			err = recvfrom(sock, rcvbuf, sizeof(struct packet_header) + input_header.datasize, MSG_DONTWAIT, NULL, 0);
			if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
				continue;
			}
			if(err < 0){
				die("Receive from", errno);
			}else if(err < input_header.datasize + sizeof(struct packet_header)){
				die("short read", err);
			}

			if(input_header.dest > MAX_HOSTS){
			//drop packet
#ifdef FORWARD_DEBUG
			printf("Dropped packet for bad dest %zu\n", input_header.dest);
#endif				
				continue;
			}else if(input_header.dest == whoami){
				//consume packet
#ifdef FORWARD_DEBUG
				printf("Consumed packet:\n");
				print_memblock(rcvbuf+sizeof(struct packet_header), input_header.datasize, 20);
#endif
				continue;
			}
			
			err = forward_packet(rcvbuf, sock, FORWARD_DATA);
			if(err < 0){
				//handle extra dropped packet stuff here
			}
			
		}else{
			//drop the packet
#ifdef FORWARD_DEBUG
			printf("Dropped packet with header type %u\n", input_header.magick);
#endif
		}
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	int err;
	

	continue_running = true;
	struct sigaction act;
	memset(&act, '\0', sizeof(act));

	act.sa_sigaction = &hdl;
	act.sa_flags = SA_SIGINFO & ~SA_RESTART;

	if (sigaction(SIGHUP, &act, NULL) == -1) {
		die("sigaction", 0);
	}

	setup(argc, argv);
	printhost(whoami);
	init_routing_table(whoami);
	
	
	print_routing_table();
	
	int sock = getsocket(hosts[whoami].routingport);
	
	pthread_rwlock_init(&routing_table_lock, NULL);


	pthread_t thread_ids[THREAD_MAX];
	err = pthread_create(&thread_ids[THREAD_ROUTING], NULL, routingthread, &sock);
	if (err != 0) {
		die("pthread_create", errno);
	}

	err = pthread_create(&thread_ids[THREAD_TIMER], NULL, timerthread, &sock);
	if (err != 0) {
		die("pthread_create", errno);
	}

	err = pthread_join(thread_ids[THREAD_ROUTING], NULL);
	if (err != 0) {
		die("pthread_join", errno);
	}

	err = pthread_join(thread_ids[THREAD_TIMER], NULL);
	if (err != 0) {
		die("pthread_join", errno);
	}

	err = pthread_rwlock_destroy(&routing_table_lock);
	assert (err == 0);

	return 0;
}

