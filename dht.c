#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "dht.h"

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
	} while (tmp->next != tmp);
	free(pseudofiles);
}

static unsigned long
hash(unsigned char *str) {
	unsigned long h = 5381;
	int c;
	while ((c = *str++)) {
		h = ((h << 5) + h) + c;
	}
	return h;
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
	} while (tmp != tmp->next);
	return false;
}
