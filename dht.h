#ifndef DHT_H
#define DHT_H

#include "routing.h"

void add(const char * const f);
bool get(const char * const f);
node next(const char * const f, const node whoami);

node dht_handle_packet(const node whoami, char* buf);

#endif
