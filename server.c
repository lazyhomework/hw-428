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

/*
TODO list
free host struct in hosts
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
		printf("Raw buffer\n");
		for(int i = 0; i < sizeof(struct packet_header); ++i){
			if(i%4 == 0) printf(" ");
			printf("%02x",rcvbuf[i]);
		}
		printf("\n");	
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
			printf("Raw buffer\n");
			for(int k = 0; k < MAX_HOSTS; ++k){
				for(size_t i = 0; i < sizeof(struct route); ++i){
					if(i%4 == 0) printf(" ");
					printf("%02x",rcvbuf[i + k*sizeof(struct route) + sizeof(struct packet_header)]);
				}
				printf("\n");
			}
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

