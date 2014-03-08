#ifndef util_unix_h
#define util_unix_h

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "switch.h"

void path2sa(path_t p, struct sockaddr_in *sa);
void sa2path(struct sockaddr_in *sa, path_t p);
packet_t util_file2packet(char *file);

// load an array of hashnames from a file and return them as a new bucket
bucket_t bucket_load(xht_t index, char *file);

#endif