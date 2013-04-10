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

#include "config.h"
#include "packets.h"
#include "routing.h"

node whoami;

void die(char* s, int err) {
	printf("%s", s);
	if (err > 0) {
		printf(" - ");
		strerror(err);
	}
	printf("\n");
	exit(err);
}

void usage(int err) {
	printf("./server -n nodeid\n");
	exit (err);
}

void setup(int argc, char* argv[]) {
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
	THREAD_MAX
};

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

pthread_rwlock_t routing_table_lock;

void init_routing_table() {
	/* Not really required to lock, but for sanity */
	pthread_rwlock_rdlock(&routing_table_lock);

	for (size_t i = 0; i < MAX_HOSTS; ++i) {
		routing_table[i].next_hop = MAX_HOSTS;
		routing_table[i].distance = INFINTITY;
		memcpy(&routing_table[i].pathentries, false , MAX_HOSTS);
	}

	for (size_t i = 0; hosts[whoami].neighbors[i] != TERMINATOR; ++i) {
		routing_table[hosts[whoami].neighbors[i]].pathentries[hosts[whoami].neighbors[i]] = true;
		routing_table[hosts[whoami].neighbors[i]].next_hop = hosts[whoami].neighbors[i];
		routing_table[hosts[whoami].neighbors[i]].distance = 1;
	}
	pthread_rwlock_unlock(&routing_table_lock);
}


void* routingthread(void* data) {
	int sock = getsocket(hosts[whoami].routingport);
	return NULL;
}


int main(int argc, char* argv[]) {
	int err;

	setup(argc, argv);
	printhost(whoami);
	init_routing_table();

	pthread_rwlock_init(&routing_table_lock, NULL);


	pthread_t thread_ids[THREAD_MAX];
	err = pthread_create(&thread_ids[THREAD_ROUTING], NULL, routingthread, NULL);
	if (err != 0) {
		die("pthread_create", errno);
	}

	err = pthread_join(thread_ids[THREAD_ROUTING], NULL);
	if (err != 0) {
		die("pthread_join", errno);
	}

	return 0;
}
