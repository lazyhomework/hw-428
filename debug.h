#ifndef DEBUG_UTIL_H
#define DEBUG_UTIL_H

#include <stdio.h>

#ifndef ROUTING_DEBUG	
	#define ROUTING_DEBUG
#endif

#ifndef TIMING_DEBUG
	//#define TIMING_DEBUG
#endif

#ifndef FORWARD_DEBUG
	#define FORWARD_DEBUG
#endif

void print_memblock(void *buf, size_t size, size_t rowwidth);
#endif
