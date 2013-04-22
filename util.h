#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "config.h"
#include "packets.h"
#include "routing.h"
#include "debug.h"


__attribute__ ((noreturn, nonnull (1))) void die(char* s, int err);
int getsocket(port p);
