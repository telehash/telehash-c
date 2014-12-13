#ifndef net_tcp4_h
#define net_tcp4_h

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "mesh.h"

// overall server
typedef struct net_tcp4_struct
{
  int server;
  int port;
  mesh_t mesh;
  xht_t pipes;
  lob_t path;
} *net_tcp4_t;

// create a new listening tcp server
net_tcp4_t net_tcp4_new(mesh_t mesh, lob_t options);
void net_tcp4_free(net_tcp4_t net);

// check all sockets for work (just for testing, use libuv or such for production instead)
net_tcp4_t net_tcp4_loop(net_tcp4_t net);

#endif

#endif
