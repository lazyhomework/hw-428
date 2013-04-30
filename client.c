#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "config.h"
#include "packets.h"
#include "debug.h"
#include "util.h"

node source;
node dest;

enum {
	CREATE,
	TEARDOWN,
	SENDDATA
} mode;

void usage(int err) {
	printf("./client -s nodeid -d nodeid -ctx\n");
	exit (err);
}

int send_packet(int sock, enum packet_type type, size_t datasize, void *data){
	int err;
	struct packet_header header;
	
	unsigned char * buffer = malloc(datasize + sizeof(struct packet_header));
	
	header.magick = type;
	header.dest = dest;
	header.prevhop = source;
	header.rout_port = hosts[source].routingport;
	header.data_port = hosts[source].dataport;
	header.ttl = MAX_PACKET_TTL;
	header.datasize = datasize;
	memcpy(buffer,&header,sizeof(struct packet_header));
	memcpy(buffer + sizeof(struct packet_header),data, datasize);
	
	size_t buffersize = sizeof(struct packet_header) + datasize;
	
	err = sendto(sock, buffer, buffersize, 0, 0, 0);
	if(err < 0){
		perror("sendto: ");
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
				source = atoi(optarg);
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

static int getsock(void) {
	int sendfd;
	struct addrinfo ai = getremotehostname(hosts[source].hostname, hosts[source].routingport);
	sendfd = socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
	if (sendfd == -1) {
		die("socket", 1);
	}
	return sendfd;
}

int main(int argc, char* argv[]) {
	setup(argc, argv);

	int fd = getsock();
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
	
	err = send_packet(fd,PACKET_CREATE, 2*sizeof(node), data);
	if(err < 0){
		die("Send to", err);
	}
	return 0;
}

