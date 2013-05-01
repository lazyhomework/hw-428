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
#include "dht.h"


static node whoami;
static port this_dataport;
static port this_routingport;
/*
TODO list
free host struct in hosts
***Done***fix unlocked table access in timer thread. tablecpy.host is ptr to mem that should be locked. soln: copy addr to stack.
***DONE***(see note in routing.h)Clear up host vs network on ports
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
	unsigned int interval = 5; //in sec
	int sock = *((int*) data);
	int buffersize = MAX_HOSTS * sizeof(struct route);
	int err;
	
	unsigned char buffer[buffersize];

	//tablecpy points to the buffer, starting immediately after the packet header.
	struct route* tablecpy = (struct route *)buffer;

		
	while(continue_running){
		sleep(interval);
		
		pthread_rwlock_wrlock(&routing_table_lock);

		//update the ttl on all routes		
		for (size_t i = 0; i < MAX_HOSTS; ++i) {
			//Don't decrement the ttl on table entry for self
			if(i != whoami && (routing_table[i].ttl -= interval) <= 0){
				remove_entry(whoami, i);
			}
		}
		//copy the routing table to buffer.
		memcpy(tablecpy,routing_table, MAX_HOSTS * sizeof(struct route));

		pthread_rwlock_unlock(&routing_table_lock);

		
#ifdef TIMING_DEBUG
		printf("Table after update ttl \n");
		print_rt_ptr(tablecpy);
#endif
		
		//Go through list of neighbors, send the table to them.
		for (size_t i = 0; i < MAX_HOSTS; ++i) {			
			if(tablecpy[i].distance == 1 && tablecpy[i].host != NULL){
				err = send_packet(sock, PACKET_ROUTING, whoami, i, buffersize, buffer, OPTION_ROUTE);
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
				
				switch(header.magick) {
				
				case PACKET_CREATE:
					//Data order is <from> <to> so second num gets the dest.
					data_values = (node*) (rcvbuf+sizeof(struct packet_header));
					neighbor = data_values[1];
					
					if(neighbor > MAX_HOSTS){
						printf("bad dest %zu\n", neighbor);
						continue;
					}
					
					pthread_rwlock_wrlock(&routing_table_lock);
					err = add_neighbor(whoami,neighbor);
					pthread_rwlock_unlock(&routing_table_lock);
					
					if(err < 0){
						//We already had connection, no need to resend.
						printf("Ignoring connection request, connection already exists\n");
						continue;
					}
					
					//Tell new neighbor we're here.
					data_values[0] = neighbor;
					data_values[1] = whoami;
					err = send_packet(sock, PACKET_CREATE, whoami, neighbor,  2*sizeof(node),data_values,OPTION_ROUTE);
					if(err < 0){
						die("send packet",err);
					}
					
#ifdef ROUTING_DEBUG
					printf("Neighbor: %zu, whoami %zu\n",data_values[0], data_values[1]);
#endif					
					
					//Ideally I'd send the table now, but with poor function planning its too much fixing.
					break;
					
				case PACKET_TEARDOWN:
					data_values = (node*) (rcvbuf+sizeof(struct packet_header));
					neighbor = data_values[1];
					if(neighbor > MAX_HOSTS){
						printf("bad dest %zu\n", neighbor);
						continue;
					}
					
					pthread_rwlock_rdlock(&routing_table_lock);
					
					
					if(routing_table[neighbor].host != NULL){
						pthread_rwlock_unlock(&routing_table_lock);
						
						//Tell other guy to stop talking to us
						data_values[0] = neighbor;
						data_values[1] = whoami;
						err = send_packet(sock, PACKET_TEARDOWN, whoami, neighbor, 2*sizeof(node),data_values,OPTION_ROUTE);
						if(err < 0){
							die("send packet",err);
						}
						
						pthread_rwlock_wrlock(&routing_table_lock);
						
						for(int i=0; i < MAX_HOSTS; ++i){
							if(routing_table[i].next_hop == neighbor){
								remove_entry(whoami, i);
							}
						}
					}else{
						//No connection to tear down.
#ifdef ROUTING_DEBUG
						printf("No connection to break, ignoring message \n");
#endif										
					}
					
					pthread_rwlock_unlock(&routing_table_lock);
					
					break;
					
				case PACKET_SENDDATA:{
					data_values = (node*) (rcvbuf+sizeof(struct packet_header));
					neighbor = data_values[1];
					
					char *message = malloc(50 * sizeof(*message));
					fill_buffer(message, 50);
					
					err = send_packet(sock, PACKET_DATA, whoami, neighbor, 50 ,message,OPTION_DATA);
					if(err < 0 ){
						if(err == EFORWARD){
							printf("No path known to dest %zu \n", neighbor);
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
			//Should only get get direct messages on route port
				/*
				printf("forwarding packet: ");
				err = forward_packet(rcvbuf,sock,whoami,OPTION_ROUTE);
				if(err == EFORWARD){
					printf("Could not forward packet, cannot reach dest %zu\n", header.dest);
				}
				*/
			}
		
		}else if(header.magick == PACKET_ROUTING){
				
			pthread_rwlock_wrlock(&routing_table_lock);
			
			path = (struct route *) (rcvbuf+sizeof(struct packet_header));
			
#ifdef ROUTING_DEBUG
			//printf("Raw buffer\n");
			//print_memblock(rcvbuf,sizeof(struct route)*MAX_HOSTS, sizeof(struct route));
			
			printf("Old routing table\n");				
			print_routing_table();
			printf("Table reveived from %zu\n", header.prevhop);
			print_rt_ptr(path);
#endif
			

			//Update our info on how to contact sender on every routing packet
			if(routing_table[header.prevhop].host != NULL){
				/*
				routing_table[header.prevhop].host->sin_family = addr.sin_family;
				routing_table[header.prevhop].host->sin_port = htons(header.rout_port);
				routing_table[header.prevhop].host->sin_addr.s_addr = addr.sin_addr.s_addr;
				routing_table[header.prevhop].data_port = header.data_port;
				*/
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
			printf("new routing table updated from host #%zu\n", header.prevhop);
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
	bool valid_packet = true, send_icmp = false;
	
	unsigned char rcvbuf[MAX_PACKET] = { 0 };

	struct packet_header input_header;
	struct packet_header *out_header = (struct packet_header *) rcvbuf;
	
	struct icmp_payload icmp;
	struct icmp_payload *icmp_data;
	while(continue_running){
	
		//non-blocking read, peeks at data
		err = recvfrom(sock, &input_header, sizeof(struct packet_header), MSG_DONTWAIT|MSG_PEEK, NULL, 0);
		if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		if(err < 0){
			die("Receive from", errno);
		}else if(err < sizeof(struct packet_header)){
			die("short read, header", err);
		}
		
		#ifdef FORWARD_DEBUG
		print_pack_h(&input_header);
		#endif
		
		//non-blocking read, consumes data
		err = recvfrom(sock, rcvbuf, sizeof(struct packet_header) + input_header.datasize, MSG_DONTWAIT, NULL, 0);
		if (err < 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		if(err < 0){
			die("Receive from", errno);
		}else if(err < input_header.datasize + sizeof(struct packet_header)){
			die("short read, body", err);
		}

		if (input_header.magick == PACKET_DHT_GET ||
		    input_header.magick == PACKET_DHT_PUT) {
			node fwdto = dht_handle_packet(whoami, rcvbuf);
			if (fwdto == whoami) {
				continue;
			}
			else {
				valid_packet = true;
				out_header->dest = fwdto;
			}
		}
		
		icmp.source = whoami;
		icmp.dest = input_header.source;

		if(input_header.magick != PACKET_DATA && input_header.magick != PACKET_ICMP){
			//drop the packet
			printf("Dropped packet with header type %u\n", input_header.magick);
			icmp.type = ICMP_ROUTERR;
			valid_packet = false;
			send_icmp = true;
			
		}else if(input_header.magick != PACKET_ICMP){
			send_icmp = true;
		}
		
		
		if(input_header.dest > MAX_HOSTS){
			//drop packet
			printf("Dropped packet for bad dest %zu\n", input_header.dest);
			icmp.type = ICMP_ROUTERR;
			valid_packet = false;
		}
		
		if(input_header.dest == whoami && valid_packet){
			//consume packet
			if(input_header.magick == PACKET_ICMP){
				icmp_data = (struct icmp_payload *) (rcvbuf + sizeof(struct packet_header));
				switch(icmp_data->type){
				case ICMP_PING:
					icmp.type = ICMP_PING_RESP;
					icmp.dest = icmp_data->source;
					send_icmp = true;
					valid_packet = false;
					break;
				default:
					//do something with the rest of the packet types.
					continue;
				}
			}else{
				printf("Consumed packet:\n");
				print_memblock(rcvbuf+sizeof(struct packet_header), input_header.datasize, 20);
				send_icmp = false;
				continue;
			}
		}
		
		//Update ttl, drop if 0
		if((out_header->ttl -= 1) <= 0){
			//drop packet
			printf("Dropped packet for ttl timeout.\n");
			icmp.type = ICMP_TIMEOUT;
			valid_packet = false;
		}
		
		if(!valid_packet && send_icmp){
			err = send_packet(sock, PACKET_ICMP, whoami, input_header.source, sizeof(icmp), &icmp, OPTION_DATA);
			if(err == EFORWARD){
				printf("No path known to dest %zu \n", input_header.source);
			}else if(err < 0){
				die("send packet",err);
			}
		}else if(valid_packet){
			err = forward_packet(rcvbuf, sock, whoami, OPTION_DATA);
			if(err == EFORWARD){
				printf("Could not forward packet, cannot reach dest %zu\n", input_header.dest);
			}
		}
		valid_packet = true;
		send_icmp = false;
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
	int data_sock = getsocket(hosts[whoami].dataport);
	
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

	err = pthread_create(&thread_ids[THREAD_FORWARD], NULL, forwardingthread, &data_sock);
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

	err = pthread_join(thread_ids[THREAD_FORWARD], NULL);
	if (err != 0) {
		die("pthread_join", errno);
	}

	err = pthread_rwlock_destroy(&routing_table_lock);
	assert (err == 0);

	return 0;
}

