#ifndef net_tmesh_h
#define net_tmesh_h

#include "mesh.h"
#include "epoch.h"

typedef struct net_tmesh_struct *net_tmesh_t;

// individual mote pipe local info, wrapper around a link for radio peers
typedef struct mote_struct
{
  net_tmesh_t net;
  util_chunks_t chunks;
  epochs_t es;
  link_t link;
  uint8_t z;
  pipe_t pipe;
  uint64_t sync; // when synchronized, 0 if trying to sync
  struct mote_struct *next;
} *mote_t;

// overall manager
struct net_tmesh_struct
{
  mesh_t mesh;
  mote_t motes;
  lob_t path;
  epochs_t syncs; // the ones we listen on
  epoch_t tx, rx; // soft scheduled
  epoch_t active; // hard scheduled
  epoch_t (*init)(net_tmesh_t net, epoch_t e); // callback used to initialize all new epochs, add scheduling time/cost
};

// create a new tmesh radio network bound to this mesh
net_tmesh_t net_tmesh_new(mesh_t mesh, lob_t options, epoch_t (*init)(net_tmesh_t net, epoch_t e));
void net_tmesh_free(net_tmesh_t net);

// add a sync epoch from this header, right now must be called at initialization/creation only
net_tmesh_t net_tmesh_sync(net_tmesh_t net, char *header);

// get(create) the mote for a link
mote_t net_tmesh_mote(net_tmesh_t net, link_t link);

// perform buffer management and internal soft scheduling
net_tmesh_t net_tmesh_loop(net_tmesh_t net);

// return the next hard-scheduled epoch from this given point in time
epoch_t net_tmesh_next(net_tmesh_t net, uint64_t from);


#endif
