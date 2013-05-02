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

node source;
node dest;

static enum {
	CREATE,
	TEARDOWN,
	SENDDATA
} mode;

void usage(int err) {
	printf("./client -s nodeid -d nodeid -ctx\n");
	exit (err);
}

static int client_packet(int sock, enum packet_type type, size_t datasize, void *data){
	int err;
	struct packet_header header;
	
	unsigned char * buffer = malloc(datasize + sizeof(struct packet_header));
	
	header.magick = type;
	header.dest = source;
	header.prevhop = source;
	header.source = source;
	header.rout_port = hosts[source].routingport;
	header.data_port = hosts[source].dataport;
	header.ttl = MAX_PACKET_TTL;
	header.datasize = datasize;
	memcpy(buffer,&header,sizeof(struct packet_header));
	memcpy(buffer + sizeof(struct packet_header),data, datasize);
	
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

static void setup(int argc, char* argv[]) {
	char ch;

	int required = 0x0;

	while (((ch = getopt(argc, argv, "s:d:ctxh")) != -1)) {
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

static struct addrinfo getremotehostname(char* hostname, short port) {
        struct addrinfo hints, *res0;
        int err;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET; //change this to allow IPv6 - note that other things must be changed too
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE | AI_CANONNAME;

        char portstr[5];
        snprintf(portstr, sizeof(portstr), "%hd", port);

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
	short target_port;
	
	switch(option){
	
		case OPTION_ROUTE:	
			target_port = hosts[source].routingport;
			break;
		case OPTION_DATA:
			target_port = hosts[source].dataport;
			break;
		default:
			die("Bad option to getsock()",1);
			break;
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

int ping(){
	int fd = getsock(OPTION_DATA);
	int err;
	
	char buffer[MAX_PACKET];
	
	struct icmp_payload data = {ICMP_PING,source,dest};
	struct timeval start, end;
	
	
	gettimeofday(&start, NULL);
	err = client_packet(fd,PACKET_ICMP, sizeof(data), &data);
	if(err < 0){
		die("Ping: Send to", err);
	}

	err = recv(fd, buffer, MAX_PACKET,0);
	if(err < 0){
		die("Ping: Receive", errno);
	}

	gettimeofday(&end,NULL);
	return 0;
}

int main(int argc, char* argv[]) {
	setup(argc, argv);

	int fd = getsock(OPTION_ROUTE);
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
	}
	
	err = client_packet(fd,type, 2*sizeof(node), data);
	if(err < 0){
		die("Send to", err);
	}
	return 0;
}

