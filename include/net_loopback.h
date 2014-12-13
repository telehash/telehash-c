#ifndef net_loopback_h
#define net_loopback_h

#include "mesh.h"

typedef struct net_loopback_struct
{
  pipe_t pipe;
  mesh_t a, b;
} *net_loopback_t;

// connect two mesh instances with each other for packet delivery
net_loopback_t net_loopback_new(mesh_t a, mesh_t b);
void net_loopback_free(net_loopback_t pair);

#endif
