#include <stdlib.h>
#include <stdio.h>

#include "config.h"

const int TERMINATOR=-1;


void __attribute__ ((constructor)) initconfig() {
	hosts[0] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5000, .routingport=5001, .neighbors = { 1, TERMINATOR }});
	hosts[1] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5010, .routingport=5011, .neighbors = { 0,2, TERMINATOR }});
	hosts[2] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5020, .routingport=5021, .neighbors = { 1,  TERMINATOR }});
	/*
	hosts[3] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5030, .routingport=5031, .neighbors = { 5 , TERMINATOR }});
	hosts[4] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5040, .routingport=5041, .neighbors = { 1, TERMINATOR }});
	hosts[5] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5050, .routingport=5051, .neighbors = { 4, 6, TERMINATOR }});
	hosts[6] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5060, .routingport=5061, .neighbors = { TERMINATOR }});
	*/
}

void printhost(size_t n) {
	
	printf("Host #%d (on %s) with data port %hu and routing port %hu with neighbors: ", n, hosts[n].hostname, hosts[n].dataport, hosts[n].routingport);
	for (size_t i = 0; hosts[n].neighbors[i] != TERMINATOR; ++i) {
		printf("%d ", hosts[n].neighbors[i]);
	}
	printf("\b\n");
}
