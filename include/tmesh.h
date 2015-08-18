#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"
#include "tmesh_epoch.h"

typedef struct tmesh_struct *tmesh_t;

// individual mote pipe local info, wrapper around a link for radio peers
typedef struct mote_struct
{
  tmesh_t tm;
  util_chunks_t chunks;
  epoch_t epochs;
  epoch_t syncs; // only when synchronizing
  epoch_t tx, rx; // currently active ones only
  link_t link;
  uint8_t z;
  pipe_t pipe;
  uint64_t sync; // when synchronized, 0 if trying to sync
  struct mote_struct *next;
} *mote_t;

// overall manager
struct tmesh_struct
{
  mesh_t mesh;
  mote_t motes;
  lob_t path, dim;
  epoch_t disco; // only when discovery is enabled
  epoch_t syncs; // the ones we listen on
  epoch_t bases; // only temporary tx/rx base epochs
  mote_t inits; // stub temp motes for handshake exchange only to bootstrap links
  knock_t tx, rx; // soft scheduled
  knock_t active; // hard scheduled
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options);
void tmesh_free(tmesh_t tm);

// add a sync epoch from this medium, must be called at initialization/creation only
tmesh_t tmesh_sync(tmesh_t tm, char *medium);

// become discoverable by anyone with this medium and network, pass NULL to reset all
tmesh_t tmesh_discoverable(tmesh_t tm, char *medium, char *network);

// get(create) the mote for a link
mote_t tmesh_mote(tmesh_t tm, link_t link);

// perform buffer management and internal soft scheduling
tmesh_t tmesh_loop(tmesh_t tm);

// return the next hard-scheduled knock from this given point in time
knock_t tmesh_next(tmesh_t tm, uint64_t from);

///////////////////
// radio devices are single task responsible for all the epochs in one or more mediums
typedef struct radio_struct
{
  // return energy cost, or 0 if unknown medium
  uint32_t (*energy)(mesh_t mesh, uint8_t medium[6]);

  // used to initialize all new epochs, add medium scheduling time/cost and channels
  void* (*bind)(mesh_t mesh, epoch_t e, uint8_t medium[6]);

  // when an epoch is free'd, in case there's any device structures
  epoch_t (*free)(mesh_t mesh, epoch_t e);
  
  // list of all epochs bound to this radio for scheduling
  epoch_t bound;

} *radio_t;

#define RADIOS_MAX 1
extern radio_t radio_devices[]; // all of em

// add/set a new device
radio_t radio_device(radio_t device);


#endif
