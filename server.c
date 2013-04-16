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

#include "config.h"
#include "packets.h"
#include "routing.h"
#include "debug.h"


static node whoami;
static port this_dataport;
static port this_routingport;
/*
TODO list
free host struct in hosts
fix unlocked table access in timer thread. tablecpy.host is ptr to mem that should be locked. soln: copy addr to stack.
*/

static void die(char* s, int err) {
	printf("%s", s);
	if (err > 0) {
		printf(" - ");
		strerror(err);
	}
	printf("\n");
	exit(err);
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
	THREAD_MAX
};

static int getsocket(port p) {
	int listenfd;
	int err;
	struct sockaddr_in servaddr;

	listenfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (listenfd == -1) {
		die("socket", errno);
	}

	printf("Obtained listen socket id %d\n", listenfd);
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(p);

	err = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (err == -1) {
		die("bind", errno);
	}
	return listenfd;
}


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

		
	while(1){
		sleep(interval);
		
		pthread_rwlock_wrlock(&routing_table_lock);

		//update the ttl on all routes		
		for (size_t i = 0; i < MAX_HOSTS; ++i) {
			if((routing_table[i].ttl -= interval) <= 0){
				routing_table[i].next_hop = whoami;
				routing_table[i].distance = INFINTITY;
				routing_table[i].ttl = MAX_ROUTE_TTL;
				free(routing_table[i].host);
				routing_table[i].host = NULL;
				
				memset(routing_table[i].pathentries, false , MAX_HOSTS);
				routing_table[i].pathentries[0] = TERMINATOR;
			}
		}
		//copy the routing table to buffer.
		memcpy(tablecpy,routing_table, MAX_HOSTS * sizeof(struct route));
		
		pthread_rwlock_unlock(&routing_table_lock);
		
		//Go through list of neighbors, send the table to them.
		for (size_t i = 0; i < MAX_HOSTS; ++i) {
				
				if(tablecpy[i].distance == 1){
				
					neighbor = i;
				
					header.dest = neighbor;
					memcpy(buffer,&header,sizeof(struct packet_header));

#ifdef TIMING_DEBUG
					printf("Sending packet: ");
					print_pack_h((struct packet_header*)buffer);
					print_rt_ptr(tablecpy);
#endif

					err = sendto(sock, buffer, buffersize, 0, (struct sockaddr *) tablecpy[i].host, sizeof(targetaddr));
					if(err < 0){
						die("Timer send",errno);
					}
				}
		}
		
	}
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
	

	while(1){
	
		//blocking read, peeks at data
		err = recvfrom(sock, rcvbuf, sizeof(struct packet_header), MSG_PEEK,(struct sockaddr *)&addr, &addrsize);
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
			err = recvfrom(sock, rcvbuf, sizeof(struct packet_header) + header.datasize, 0, (struct sockaddr *) &addr, &addrsize);
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
			routing_table[header.prevhop].data_port = htons(header.data_port);
			
			for(size_t i = 0; i < MAX_HOSTS; ++i){				
				//we're not part of path AND ( Distance is shorter OR table came from next hop on path )
				if( !path[i].pathentries[whoami] 
						&& (path[i].distance +1 < routing_table[i].distance
						|| routing_table[i].next_hop == header.prevhop ) ){
				
					routing_table[i].distance = path[i].distance + 1;
					routing_table[i].next_hop = header.prevhop;
					routing_table[i].ttl = MAX_ROUTE_TTL;
					memcpy(routing_table[i].pathentries, path[i].pathentries, MAX_HOSTS);
					routing_table[i].pathentries[whoami] = true;
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
	node next_hop;
	size_t packet_size = 0;
	
	unsigned char rcvbuf[MAX_PACKET] = { 0 };

	struct sockaddr_in addr;
	struct packet_header input_header;
	struct packet_header* out_header = (struct packet_header*) rcvbuf;
	
	while(1){
	
		//blocking read, peeks at data
		err = recvfrom(sock, &input_header, sizeof(struct packet_header), MSG_PEEK, NULL, 0);
		if(err < 0){
			die("Receive from", errno);
		}else if(err < sizeof(struct packet_header)){
			die("short read", err);
		}
		
		//Possibly add other error checking here
		if(input_header.magick == PACKET_DATA){
			
			//blocking read, consumes data
			err = recvfrom(sock, rcvbuf, sizeof(struct packet_header) + input_header.datasize, 0, NULL, 0);
			if(err < 0){
				die("Receive from", errno);
			}else if(err < input_header.datasize){
				die("short read", err);
			}

			if(input_header.dest > MAX_HOSTS){
				//drop packet
#ifdef FORWARD_DEBUG
			printf("Dropped packet for bad dest %u\n", input_header.dest);
#endif				
				continue;
			}else if(input_header.dest == whoami){
				//consume packet
				continue;
			}
			
			if((out_header->ttl -= 1) <= 0){
				//drop packet
				continue;
			}
			
			out_header->prevhop = whoami;
			out_header->data_port = this_dataport;
			out_header->rout_port = htons(this_routingport);
			
			pthread_rwlock_rdlock(&routing_table_lock);


			if(routing_table[input_header.dest].distance < INFINTITY){
				next_hop = routing_table[input_header.dest].next_hop;
				
				/*Need to distiguish between data and routing!*/
				addr.sin_family = routing_table[next_hop].host->sin_family;
				addr.sin_port = htons(routing_table[next_hop].host->sin_port);
				addr.sin_addr.s_addr = routing_table[next_hop].host->sin_addr.s_addr;
			}else{
				//routing error, we can't forward this packet. Drop it
#ifdef FORWARD_DEBUG
				printf("Dropped packet for routing error, can't forward. Dest: %u\n", input_header.dest);
#endif				
			}
			
			pthread_rwlock_unlock(&routing_table_lock);
			
			packet_size = sizeof(struct packet_header) + input_header.datasize;
			err = sendto(sock, rcvbuf, packet_size, 0, (struct sockaddr *) &addr, sizeof(addr));
			if(err < 0){
				die("Forward packet send",errno);
			}
			
		}else{
			//drop the packet
#ifdef FORWARD_DEBUG
			printf("Dropped packet with header type %u\n", input_header.magick);
#endif
		}
	}
}

int main(int argc, char* argv[]) {
	int err;
	
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

