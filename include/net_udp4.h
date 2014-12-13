#ifndef net_udp4_h
#define net_udp4_h

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "mesh.h"

// overall server
typedef struct net_udp4_struct
{
  int server;
  int port;
  mesh_t mesh;
  xht_t pipes;
  lob_t path; // to us
} *net_udp4_t;

// create a new listening udp server
net_udp4_t net_udp4_new(mesh_t mesh, lob_t options);
void net_udp4_free(net_udp4_t net);

// receive a packet into this mesh
net_udp4_t net_udp4_receive(net_udp4_t net);

#endif

#endif
