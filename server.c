#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "packets.h"
#include "routing.h"

node whoami;

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

int main(int argc, char* argv[]) {
	setup(argc, argv);
	printhost(whoami);
	return 0;
}
