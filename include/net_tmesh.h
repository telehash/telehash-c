#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"
#include "epoch.h"

typedef struct tmesh_struct *tmesh_t;

// individual mote pipe local info, wrapper around a link for radio peers
typedef struct mote_struct
{
  tmesh_t tm;
  util_chunks_t chunks;
  epoch_t active;
  epoch_t syncs;
  knock_t tx, rx; // only one of each ever exists, is re-set each loop
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
  lob_t path;
  mote_t disco; // virtual mote, only when discovery is enabled
  epoch_t syncs; // the ones we listen on
  knock_t tx, rx; // soft scheduled
  knock_t active; // hard scheduled
  epoch_t (*init)(tmesh_t tm, epoch_t e); // callback used to initialize all new epochs, add scheduling time/cost and channels
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options, epoch_t (*init)(tmesh_t tm, epoch_t e));
void tmesh_free(tmesh_t tm);

// add a sync epoch from this header, right now must be called at initialization/creation only
tmesh_t tmesh_sync(tmesh_t tm, char *header);

// become discoverable by anyone with this epoch id, pass NULL to reset all
tmesh_t tmesh_discoverable(tmesh_t tm, char *id);

// get(create) the mote for a link
mote_t tmesh_mote(tmesh_t tm, link_t link);

// perform buffer management and internal soft scheduling
tmesh_t tmesh_loop(tmesh_t tm);

// return the next hard-scheduled epoch from this given point in time
epoch_t tmesh_next(tmesh_t tm, uint64_t from);


#endif
