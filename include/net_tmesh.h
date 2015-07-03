#ifndef net_tmesh_h
#define net_tmesh_h

#include "mesh.h"
#include "epoch.h"

typedef struct net_tmesh_struct *net_tmesh_t;

// individual pipe local info
typedef struct pipe_tmesh_struct
{
  net_tmesh_t net;
  util_chunks_t chunks;
  epoch_t *list; // resized array
} *pipe_tmesh_t;

// overall manager
struct net_tmesh_struct
{
  mesh_t mesh;
  xht_t pipes;
  lob_t path;
  epoch_t *lost; // resized array
  epoch_t tx, rx; // all active, master lists
};

// create a new tmesh radio network bound to this mesh
net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options);
void net_tmesh_free(net_tmesh_t net);

// perform buffer management and internal soft scheduling
net_tmesh_t net_tmesh_loop(net_tmesh_t net);

// return the next hard-scheduled epoch from this given point in time
epoch_t net_tmesh_next(net_tmesh_t net, uint64_t from);

/* discussion on flow

* every pipe is a mote/link
* lost is a virtual mote that is constantly reset
* each mote has it's own time sync base to derive window counters
* any mote w/ a tx knock ready is priority
* upon tx, lost rx is reset
* when no tx, the next rx is always scheduled
* when no tx for some period of time, generate one on the lost epoch
* when in discovery mode, it is a virtual mote that is always tx'ing

*/

#endif
