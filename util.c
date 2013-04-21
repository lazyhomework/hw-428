#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "util.h"

void die(char* s, int err) {
	printf("%s", s);
	if (err > 0) {
		printf(" - ");
		strerror(err);
	}
	printf("\n");
	exit(err);
}
