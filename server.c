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
				
				if(routing_table[i].host != NULL){
						free(routing_table[i].host);
						routing_table[i].host = NULL;
				}
				
				memset(routing_table[i].pathentries, false , MAX_HOSTS);
			}
		}
		//copy the routing table to buffer.
		memcpy(tablecpy,routing_table, MAX_HOSTS * sizeof(struct route));
		

		
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
		pthread_rwlock_unlock(&routing_table_lock);
	}
	return NULL;
}


/*
Option is 0 to send to routing port, 1 for data.
Buffer must have a packet_header at the start of it, with accurate datasize field.
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
			case OPTION_DATA:
				addr.sin_port = htons(routing_table[next_hop].data_port);
				break;
			case OPTION_ROUTE:
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
		
		err = recvfrom(sock, rcvbuf, sizeof(struct packet_header) + header.datasize, MSG_DONTWAIT, (struct sockaddr *) &addr, &addrsize);
		if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		if(err < 0){
			die("Receive from", errno);
		}else if(err < header.datasize){
			die("short read", err);
		}

		if(header.magick == PACKET_HELLO){
			//not needed with dist vector routing
			die("Sould not receive hello",-1);
		
		}else if(header.magick == PACKET_CREATE || header.magick == PACKET_TEARDOWN || header.magick == PACKET_SENDDATA){
			
			if(header.dest == whoami){
				
				node neighbor;
				node *data_values;
				struct hostent *temphost;
				
				switch(header.magick){
				
				case PACKET_CREATE:
					//Data order is <from> <to> so second num gets the dest.
					data_values = (node*) (rcvbuf+sizeof(struct packet_header));
					neighbor = data_values[1];
					
					if(neighbor < 0 || neighbor > MAX_HOSTS){
						printf("bad dest %u\n", neighbor);
						continue;
					}
					
					pthread_rwlock_wrlock(&routing_table_lock);
					err = add_neighbor(whoami,neighbor);
					pthread_rwlock_unlock(&routing_table_lock);
					
					if(err < 0){
						//We already had connection, no need to resend.
						continue;
					} 
					//Tell new neighbor we're here.
					data_values[0] = neighbor;
					data_values[1] = whoami;
					err = send_packet(sock, PACKET_CREATE, neighbor, whoami, 2*sizeof(node),data_values,OPTION_ROUTE);
					if(err < 0){
						die("send packet",err);
					}
					//Ideally I'd send the table now, but with poor function planning its too much copy paste.
					break;
					
				case PACKET_TEARDOWN:
					data_values = (node*) (rcvbuf+sizeof(struct packet_header));
					neighbor = data_values[1];
					if(neighbor < 0 || neighbor > MAX_HOSTS){
						printf("bad dest %u\n", neighbor);
						continue;
					}
					
					pthread_rwlock_wrlock(&routing_table_lock);
					
					
					if(routing_table[neighbor].host != NULL){
						free(routing_table[neighbor].host);
						routing_table[neighbor].host = NULL;
					}else{
						//No connection to tear down.
						continue;
					}
					
					routing_table[neighbor].next_hop = whoami;
					routing_table[neighbor].distance = INFINTITY;
					routing_table[neighbor].ttl = MAX_ROUTE_TTL;
					memset(routing_table[neighbor].pathentries, false , MAX_HOSTS);
					
					pthread_rwlock_unlock(&routing_table_lock);
					
					//Tell other guy to stop talking to us
					data_values[0] = neighbor;
					data_values[1] = whoami;
					err = send_packet(sock, PACKET_TEARDOWN, neighbor, whoami, 2*sizeof(node),data_values,OPTION_ROUTE);
					if(err < 0){
						die("send packet",err);
					}
									
					break;
					
				case PACKET_SENDDATA:{
					data_values = (node*) (rcvbuf+sizeof(struct packet_header));
					neighbor = data_values[1];
					
					unsigned char *message = malloc(50 * sizeof(*message));
					fill_buffer(message, 50);
					
					err = send_packet(sock, PACKET_DATA, neighbor, whoami, 50 ,message,OPTION_DATA);
					if(err < 0 ){
						if(err == EFORWARD){
							printf("No path known to dest %u \n", neighbor);
							continue;
						}
						else{
							die("send packet",err);
						}
					}
					
					free(message);
					break;
				}
				}
			
			}else{
				forward_packet(rcvbuf,sock,OPTION_ROUTE);
			}
		
		}else if(header.magick == PACKET_ROUTING){
				
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
			

			//Update our info on how to contact sender on every routing packet
			if(routing_table[header.prevhop].host != NULL){
				routing_table[header.prevhop].host->sin_family = addr.sin_family;
				routing_table[header.prevhop].host->sin_port = htons(header.rout_port);
				routing_table[header.prevhop].host->sin_addr.s_addr = addr.sin_addr.s_addr;
				routing_table[header.prevhop].data_port = header.data_port;
			}else{
				//ignore tables from hosts we havn't connected to.
				continue;
			}
			
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


/*
Currently has no buffer, forwards as fast a possible.
Buffer is limited by underlying socket
*/
static void* forwardingthread(void *data){
	int sock = *((int*)data);
	int err = 0;
	
	unsigned char rcvbuf[MAX_PACKET] = { 0 };

	struct packet_header input_header;
	
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
			
			err = forward_packet(rcvbuf, sock, OPTION_DATA);
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

