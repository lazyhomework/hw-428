#ifndef DEBUG_UTIL_H
#define DEBUG_UTIL_H

#include <stdio.h>

#ifndef ROUTING_DEBUG	
	#define ROUTING_DEBUG
#endif

#ifndef TIMING_DEBUG
	#define TIMING_DEBUG
#endif

<<<<<<< HEAD
#ifndef FORWARD_DEBUG
	#define FORWARD_DEBUG
#endif

=======
>>>>>>> 161ab38b3944fd78be32f852c9696a98f4c56f6f
void print_memblock(void *buf, size_t size, size_t rowwidth);
#endif
