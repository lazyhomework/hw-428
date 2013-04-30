#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "util.h"

void die(char* s, int err) {
	printf("%s", s);
	if (err > 0) {
		printf(" - ");
		strerror(err);
	}
	printf("\n");
	exit(err);
}

int getsocket(port p) {
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
