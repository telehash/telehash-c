#ifndef net_tmesh_h
#define net_tmesh_h

#include "mesh.h"
#include "epoch.h"

typedef struct net_tmesh_struct *net_tmesh_t;

// individual mote pipe local info
typedef struct mote_struct
{
  net_tmesh_t net;
  util_chunks_t chunks;
  epochs_t es;
  link_t link;
  uint8_t z;
  pipe_t pipe;
  struct mote_struct *next;
} *mote_t;

// overall manager
struct net_tmesh_struct
{
  mesh_t mesh;
  mote_t motes;
  lob_t path;
  epochs_t lost;
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

* every mote is 1:1 to a link
* lost is a virtual mote that is constantly reset
* each mote has it's own time sync base to derive window counters
* any mote w/ a tx knock ready is priority
* upon tx, lost rx is reset
* when no tx, the next rx is always scheduled
* when no tx for some period of time, generate one on the lost epoch
* when in discovery mode, it is a virtual mote that is always tx'ing

*/

#endif
