#include <stdlib.h>
#include <stdio.h>

#include "config.h"

struct host hosts[MAX_HOSTS];

const int TERMINATOR=-1;

/*
connections must be symetric to work
*/
static void __attribute__ ((constructor)) initconfig() {
	hosts[0] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5000, .routingport=5001, .neighbors = { 1,3,TERMINATOR }});
	hosts[1] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5010, .routingport=5011, .neighbors = { 0,2,4, TERMINATOR }});
	hosts[2] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5020, .routingport=5021, .neighbors = { 1,3, TERMINATOR }});
	hosts[3] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5030, .routingport=5031, .neighbors = { 2,0 , TERMINATOR }});
	hosts[4] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5040, .routingport=5041, .neighbors = { 1,5, TERMINATOR }});
	
	hosts[5] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5050, .routingport=5051, .neighbors = { 4, TERMINATOR }});
	/*
	hosts[6] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5060, .routingport=5061, .neighbors = { TERMINATOR }});
	hosts[7] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5070, .routingport=5071, .neighbors = { TERMINATOR }});
	hosts[8] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5080, .routingport=5081, .neighbors = { TERMINATOR }});
	hosts[9] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5090, .routingport=5091, .neighbors = { TERMINATOR }});
	hosts[10] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5100, .routingport=5101, .neighbors = { TERMINATOR }});
	hosts[11] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5110, .routingport=5111, .neighbors = { TERMINATOR }});
	hosts[12] = ((struct host){ .hostname="remote07.cs.binghamton.edu", .dataport=5120, .routingport=5121, .neighbors = { TERMINATOR }});
	*/
}

void printhost(size_t n) {
	
	printf("Host #%zu (on %s) with data port %hu and routing port %hu with neighbors: ", n, hosts[n].hostname, hosts[n].dataport, hosts[n].routingport);
	for (size_t i = 0; hosts[n].neighbors[i] != TERMINATOR; ++i) {
		printf("%d ", hosts[n].neighbors[i]);
	}
	printf("\b\n");
}
