#ifndef DEBUG_UTIL_H
#define DEBUG_UTIL_H

#include <stdio.h>
#include <sys/resource.h>

#ifndef ROUTING_DEBUG	
	#define ROUTING_DEBUG
#endif

#ifndef TIMING_DEBUG
	//#define TIMING_DEBUG
#endif

#ifndef FORWARD_DEBUG
	#define FORWARD_DEBUG
#endif

#ifndef CORE_DUMP_DEBUG
	#define CORE_DUMP_DEBUG
#endif

void print_memblock(void *buf, size_t size, size_t rowwidth);
void init_debug();

extern int debug_packets;
extern int debug_routing;
extern int debug_timing;
extern int debug_forward;
#endif
