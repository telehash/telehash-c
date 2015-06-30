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

// create a new tmesh radio network bound to this mesh
net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options);
void net_tmesh_free(net_tmesh_t net);

// perform buffer management and soft scheduling
net_tmesh_t net_tmesh_loop(net_tmesh_t net);

// return the next hard-scheduled epoch from this given point in time
epoch_t net_tmesh_next(net_tmesh_t net, uint64_t from);

#endif
