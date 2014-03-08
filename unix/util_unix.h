#ifndef util_unix_h
#define util_unix_h

#include <sys/socket.h>
#include <arpa/inet.h>

#include "path.h"
#include "util.h"

void path2sa(path_t p, struct sockaddr_in *sa);
void sa2path(struct sockaddr_in *sa, path_t p);

#endif