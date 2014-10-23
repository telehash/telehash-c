#ifndef net_pair_h
#define net_pair_h

#include "mesh.h"

typedef struct net_pair_struct
{
  pipe_t pipe;
  mesh_t a, b;
} *net_pair_t;

// connect two mesh instances with each other for packet delivery
net_pair_t net_pair_new(mesh_t a, mesh_t b);
void net_pair_free(net_pair_t pair);

#endif