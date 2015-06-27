#ifndef net_tmesh_h
#define net_tmesh_h

#include "mesh.h"
#include "epoch.h"

// overall manager
typedef struct net_tmesh_struct
{
  mesh_t mesh;
  xht_t pipes;
  lob_t path;
  epoch_t lost;
} *net_tmesh_t;

// create a new listening tcp server
net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options);
void net_tmesh_free(net_tmesh_t net);

net_tmesh_t net_tmesh_loop(net_tmesh_t net);

#endif
