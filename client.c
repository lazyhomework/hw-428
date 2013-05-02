#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "client.h"
#include "config.h"
#include "packets.h"
#include "debug.h"
#include "util.h"

static struct ping_ret ping_once(size_t ttl);

node source;
node dest;

static port route_port, data_port;
static int route_fd, data_fd;

static enum {
	CREATE,
	TEARDOWN,
	SENDDATA,
	PING,
	TRACE
} mode;

void usage(int err) {
	printf("./client -s nodeid -d nodeid -ctxpr\n");
	exit (err);
}

static int client_packet_full(int sock, enum packet_type type, size_t ttl, enum send_type mode, size_t datasize, void *data){
	ssize_t err;
	struct packet_header header;
	
	unsigned char * buffer = malloc(datasize + sizeof(struct packet_header));
	
	switch(mode){
	case SEND_DIRECT:
		header.dest = source;
		break;
	case SEND_PROXY:
		header.dest = dest;
		break;
	}
	header.source = source;
	header.prevhop = CLIENT_NODE;
	header.magick = type;
	header.rout_port = route_port;
	header.data_port = data_port;
	header.ttl = ttl;
	header.datasize = datasize;
	memcpy(buffer,&header,sizeof(struct packet_header));
	
	if(datasize > 0){
		memcpy(buffer + sizeof(struct packet_header),data, datasize);
	}
	
	print_pack_h(&header);
	
	size_t buffersize = sizeof(struct packet_header) + datasize;
	
	err = send(sock, buffer, buffersize, 0);
	if(err < 0){
		perror("send: ");
		free(buffer);
		return -1;
	}
	
	free(buffer);
	return 0;

}
static int client_packet(int sock, enum packet_type type, enum send_type mode, size_t datasize, void *data){
	return client_packet_full(sock,type,MAX_PACKET_TTL,mode,datasize,data);
}

static void setup(int argc, char* argv[]) {
	char ch;

	int required = 0x0;

	while (((ch = getopt(argc, argv, "s:d:ctxhpr")) != -1)) {
		switch (ch) {
			case 's':
				required |= 0x1;
				source = atoi(optarg);
				break;
			case 'd':
				required |= 0x2;
				dest = atoi(optarg);
				break;

			case 'c': /* create link */
				required |= 0x4;
				mode = CREATE;
				break;
			case 't': /* teardown link */
				required |= 0x4;
				mode = TEARDOWN;
				break;
			case 'x': /* send data */
				required |= 0x4;
				mode = SENDDATA;
				break;
			case 'p': /* ping */
				required |= 0x4;
				mode = PING;
				break;
			case 'r': /*Trace route*/
				required |= 0x4;
				mode = TRACE;
				break;
			case 'h':
				usage(0);
				break;
			case '?':
			default:
				usage(1);
		}
	}
	if (required != (0x1 | 0x2 | 0x4)) {
		usage(2);
	}
}

static void init_sockets(){
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	
	data_fd = getsock(OPTION_DATA);
	route_fd = getsock(OPTION_ROUTE);

	if(getsockname(data_fd, (struct sockaddr*)&addr, &len) < 0){
		perror("getsockname");
		die("getsockname",-1);
	}
	
	//leave as network;
	data_port = addr.sin_port;
	
	len = sizeof(addr);	
	
	if(getsockname(route_fd, (struct sockaddr*)&addr, &len) < 0){
		perror("getsockname");
		die("getsockname",-1);
	}
	
	//leave as network;
	route_port = addr.sin_port;
}

static struct addrinfo getremotehostname(char* hostname, port p) {
        struct addrinfo hints, *res0;
        int err;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET; //change this to allow IPv6 - note that other things must be changed too
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE | AI_CANONNAME;

        char portstr[5];
        snprintf(portstr, sizeof(portstr), "%hd", p);

        err = getaddrinfo(hostname, portstr, &hints, &res0);
        if (err != 0) {
                printf("%s", gai_strerror(err));
                die("getaddrinfo",1);
        }
        printf("attempting to connect to %s:%d\n", res0->ai_canonname, ntohs(((struct sockaddr_in*)res0->ai_addr)->sin_port));
        return *res0;

}

//#define stringy(s) #s

static int getsock(int option) {
	int sendfd, err;
	port target_port;
	
	switch(option){
	
		case OPTION_ROUTE:	
			target_port = hosts[source].routingport;
			break;
		case OPTION_DATA:
			target_port = hosts[source].dataport;
			break;
		default:
			die("Bad option to getsock()",1);
	}
	
	struct addrinfo ai = getremotehostname(hosts[source].hostname, target_port );
	sendfd = socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
	if (sendfd == -1) {
		die("socket", 1);
	}
	err = connect(sendfd, ai.ai_addr, ai.ai_addrlen);
	if (err < 0) {
		die("connect", err);
	}
	
	return sendfd;
}

static void trace_route(){
	size_t hops = 1;
	int err = client_packet(route_fd,PACKET_CLI_CON, SEND_DIRECT, 0, 0);
	if(err < 0){
		die("Ping: Send to", err);
	}
	
	struct ping_ret pinginfo;
	do{
		pinginfo = ping_once(hops);
		printf("Ping from %zu to %zu is %lld microseconds over %zu hop(s)\n", CLIENT_NODE, pinginfo.reached, pinginfo.time, hops);
		hops++;
	} while(pinginfo.reached != dest && hops <= MAX_PACKET_TTL);

	
	err = client_packet(route_fd,PACKET_CLI_DIS, SEND_DIRECT, 0, 0);
	if(err < 0){
		die("Ping: Send to", err);
	}
}
static int ping(){
	int err = client_packet(route_fd,PACKET_CLI_CON, SEND_DIRECT, 0, 0);
	if(err < 0){
		die("Ping: Send to", err);
	}
	
	struct ping_ret pinginfo = ping_once(MAX_PACKET_TTL);
	
	printf("Ping from %zu to %zu is %lld microseconds\n", source, dest, pinginfo.time);
	
	err = client_packet(route_fd,PACKET_CLI_DIS, SEND_DIRECT, 0, 0);
	if(err < 0){
		die("Ping: Send to", err);
	}
	
	return 0;
}

/*
returns the time diff in us from pinging dest from source
Target server must be proxy before calling. Will block forever if not :)
*/
static struct ping_ret ping_once(size_t ttl){
	int err;
	
	char buffer[MAX_PACKET];
	
	struct icmp_payload data = {ICMP_PING,source,dest};
	struct timeval start, end;
	struct ping_ret retval;
	struct packet_header *header = (struct packet_header*) buffer;
	
	gettimeofday(&start, NULL);
	
	err = client_packet_full(data_fd,PACKET_ICMP, ttl, SEND_PROXY, sizeof(data), &data);
	if(err < 0){
		die("Ping: Send to", err);
	}

	err = recv(data_fd, buffer, MAX_PACKET,0);
	if(err < 0){
		die("Ping: Receive", errno);
	}

	gettimeofday(&end,NULL);
	
	retval.time = ((end.tv_sec - start.tv_sec)/1000000) + (end.tv_usec - start.tv_usec);
	retval.reached = header->source;
	retval.recved_head = header->magick;
	return retval;
}

int main(int argc, char* argv[]) {
	setup(argc, argv);
	init_sockets();
	
	int err;
	enum packet_type type;
	
	node data[2];
	data[0] = source;
	data[1] = dest;
	
	switch(mode){
	
	case CREATE:
		type = PACKET_CREATE;
		break;
	case TEARDOWN:
		type = PACKET_TEARDOWN;
		break;
	case SENDDATA:
		type = PACKET_SENDDATA;
		break;
	case PING:
		//TODO better control flow
		ping();
		return 0;
	case TRACE:
		trace_route();
		return 0;
	}
	
	err = client_packet(route_fd,type, SEND_DIRECT, 2*sizeof(node), data);
	if(err < 0){
		die("Send to", err);
	}
	return 0;
}

