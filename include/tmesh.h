#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"

typedef struct tmesh_struct *tmesh_t;
typedef struct cmnty_struct *cmnty_t;
typedef struct epoch_struct *epoch_t;
typedef struct knock_struct *knock_t;
typedef struct medium_struct *medium_t;

// medium management w/ device driver
struct medium_struct
{
  uint8_t bin[6];
  void *device; // used by radio device driver
  uint32_t min, max; // microseconds to tx/rx, set by driver
  uint8_t chans; // number of total channels, set by driver
  uint8_t radio:4; // radio device id based on radio_devices[]
};
// community management
struct cmnty_struct
{
  tmesh_t tm;
  char *name;
  medium_t medium;
  epoch_t ping;
  link_t *links;
  epoch_t *epochs;
  pipe_t pipe;
  struct cmnty_struct *next;
  enum {NONE, PUBLIC, PRIVATE, DIRECT} type:2;
};

// maximum neighbors tracked per community
#define NEIGHBORS_MAX 8

// join a new private/public community
cmnty_t tmesh_public(tmesh_t tm, char *medium, char *name);
cmnty_t tmesh_private(tmesh_t tm, char *medium, char *name);

// add a link known to be in this community to look for
cmnty_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link);

// attempt to establish a direct connection
cmnty_t tmesh_direct(tmesh_t tm, link_t link, char *medium, uint64_t at);


// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  cmnty_t communities;
  lob_t pubim;
  uint8_t z; // our z-index
  knock_t tx, rx, any; // soft scheduled
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options);
void tmesh_free(tmesh_t tm);

// process any full packets into the mesh 
tmesh_t tmesh_loop(tmesh_t tm);

// internal soft scheduling to prep for radio_next
tmesh_t tmesh_pre(tmesh_t tm);

// radio is done, process the epoch
tmesh_t tmesh_post(tmesh_t tm, epoch_t e);


// 2^22
#define EPOCH_WINDOW (uint64_t)4194304

// knock state holder when sending/receiving
struct knock_struct
{
  knock_t next; // for temporary lists
  epoch_t epoch; // so knocks can be passed around directly
  uint32_t win; // current window id
  uint32_t chan; // current channel (< med->chans)
  uint64_t start, stop; // microsecond exact start/stop time
  util_chunks_t chunks; // actual chunk encoding
  uint8_t len:7; // <= 64
  enum {TX, RX} dir:1;
  enum {NONE, READY, DONE, ERR} state:2;
};

// individual epoch+medium state data, goal to keep <64b each on 32bit
struct epoch_struct
{
  uint8_t secret[32];
  uint64_t base; // microsecond of window 0 start
  knock_t knock; // only exists when active
  epoch_t next; // for epochs_* list utils
  medium_t medium;
  cmnty_t community; // which community the epoch belongs to
  enum {NONE, PING, ECHO, PAIR, LINK} type:4;

};

epoch_t epoch_new(mesh_t m, uint8_t medium[6]);
epoch_t epoch_free(epoch_t e);

// sync point for given window
epoch_t epoch_base(epoch_t e, uint32_t window, uint64_t at);

// set knock chan/start/stop to given window
epoch_t epoch_window(epoch_t e, uint32_t window);

// reset active knock to next window, 0 cleans out, guarantees an e->knock or returns NULL
epoch_t epoch_knock(epoch_t e, uint64_t at);

// simple array utilities
epoch_t epochs_add(epoch_t es, epoch_t e);
epoch_t epochs_rem(epoch_t es, epoch_t e);
size_t epochs_len(epoch_t es);
epoch_t epochs_free(epoch_t es);

///////////////////
// radio devices are single task responsible for all the epochs in one or more mediums
typedef struct radio_struct
{
  // return energy cost, or 0 if unknown medium, use for pre-validation/estimation
  uint32_t (*energy)(mesh_t mesh, uint8_t medium[6]);

  // initialize/get any medium scheduling time/cost and channels
  medium_t (*get)(mesh_t mesh, uint8_t medium[6]);

  // when a medium isn't used anymore, let the radio free it
  medium_t (*free)(mesh_t mesh, medium_t m);

  // perform this epoch's knock right now
  epoch_t (*knock)(mesh_t mesh, epoch_t e);
  

} *radio_t;

#define RADIOS_MAX 1
extern radio_t radio_devices[]; // all of em

// add/set a new device
radio_t radio_device(radio_t device);

// return the next hard-scheduled knock from this given point in time for this radio
epoch_t radio_next(radio_t device, tmesh_t tm, uint64_t from);

// validate medium by checking energy
uint32_t radio_energy(mesh_t m, uint8_t medium[6]);

// get the full medium
medium_t radio_medium(mesh_t m, uint8_t medium[6]);



#endif
