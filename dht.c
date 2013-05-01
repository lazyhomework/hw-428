#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "dht.h"

#define DHT_RANGE (MAX_HOSTS * 1024)

typedef unsigned long hash_t;

struct pseudofile {
	struct pseudofile *next;
	char* filename;
};
static struct pseudofile* pseudofiles = NULL;

static void  __attribute__ ((constructor)) init(void) {
	pseudofiles = malloc(sizeof(struct pseudofile));
	pseudofiles->next = pseudofiles;
	pseudofiles->filename = calloc(1,1);
}

static void __attribute__ ((destructor)) destruct(void) {
	struct pseudofile *tmp = pseudofiles;
	do {
		free(tmp->filename);
		tmp = tmp->next;
	} while (tmp->next != tmp);
	free(pseudofiles);
}

static hash_t
hash(const char * str) {
	hash_t h = 5381;
	int c;
	while ((c = *str++)) {
		h = ((h << 5) + h) + c;
	}
	return (h % DHT_RANGE);
}

node next(const char * const f, const node whoami) {
	const hash_t min = whoami *  1024;
	const hash_t max = (whoami + 1) * 1024;
	const hash_t h = hash(f);

	if (h < min) {
		if (whoami == 0) {
			return MAX_HOSTS - 1;
		}
		return whoami - 1;
	}
	else if (h >= max) {
		return (whoami + 1) % MAX_HOSTS;
	}
	else {
		return whoami;
	}
}


void add(const char* const f) {
	struct pseudofile *tmp = malloc(sizeof(struct pseudofile));
	tmp->next = pseudofiles;
	tmp->filename = strdup(f);
	pseudofiles = tmp;
}


bool get(const char * const f) {
	struct pseudofile *tmp = pseudofiles;
	do {
		if (strcmp(f, tmp->filename) == 0) {
			return true;
		}
		tmp = tmp->next;
	} while (tmp != tmp->next);
	return false;
}

/*
 * Request comes in
 * Server does match()
 *	baed on range
 *	if Server matches hash range
 *		Server does get()
 *		returns nx
 *	else
 *		server gets next hop
 *		forwards packet
 */

node dht_handle_packet(const node whoami, char* buf) {
	char* filename = (buf + sizeof(struct packet_header));
	// this is aecure - clients never lie
	node n = next(filename, whoami);
	if (n == whoami) {
		if (get(filename)) {
			//respond_to_client_true
		}
		else {
			//respond_to_cient_no_file
		}
	}
	return n;
}
