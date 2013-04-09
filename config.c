#include <stdlib.h>

#include "config.h"

void initconfig() {
	hosts[0] = ((struct host){ .hostname="gravity.local", .dataport=5000, .routingport=5001, .neighbors = { 1, 2, 3, -1 }});
	hosts[1] = ((struct host){ .hostname="gravity.local", .dataport=5010, .routingport=5011, .neighbors = { 3, -1 }});
	hosts[2] = ((struct host){ .hostname="gravity.local", .dataport=5010, .routingport=5021, .neighbors = { 3, 5 -1 }});
	hosts[3] = ((struct host){ .hostname="gravity.local", .dataport=5030, .routingport=5031, .neighbors = { 5 , -1 }});
	hosts[4] = ((struct host){ .hostname="gravity.local", .dataport=5040, .routingport=5041, .neighbors = { 1, -1 }});
	hosts[5] = ((struct host){ .hostname="gravity.local", .dataport=5050, .routingport=5051, .neighbors = { 4, 6 }});
	hosts[6] = ((struct host){ .hostname="gravity.local", .dataport=5060, .routingport=5061, .neighbors = { -1 }});

}
