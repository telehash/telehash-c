#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"

typedef struct tmesh_struct *tmesh_t;
typedef struct cmnty_struct *cmnty_t;
typedef struct epoch_struct *epoch_t;
typedef struct mote_struct *mote_t;
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
  mote_t sync; // ping+echo, only pub/priv
  mote_t motes; 
  pipe_t pipe; // one pipe per community as it's shared performance
  struct cmnty_struct *next;
  uint8_t max;
  enum {NONE, PUBLIC, PRIVATE, DIRECT} type:2;
};

// maximum neighbors tracked per community
#define NEIGHBORS_MAX 8

// join a new private/public community
cmnty_t tmesh_public(tmesh_t tm, char *medium, char *name);
cmnty_t tmesh_private(tmesh_t tm, char *medium, char *name);

// add a link known to be in this community to look for
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link);

// attempt to establish a direct connection
cmnty_t tmesh_direct(tmesh_t tm, link_t link, char *medium, uint64_t at);


// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  cmnty_t coms;
  lob_t pubim;
  uint8_t z; // our z-index
  // TODO, add callback hooks for sorting/prioritizing energy usage
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options);
void tmesh_free(tmesh_t tm);

// process any full packets into the mesh 
tmesh_t tmesh_loop(tmesh_t tm);

// internal soft scheduling to prep for radio_next
tmesh_t tmesh_prep(tmesh_t tm, uint64_t from);

// 2^22
#define EPOCH_WINDOW (uint64_t)4194304

// mote state tracking
struct mote_struct
{
  link_t link; // when known
  epoch_t epochs; // sorted order
  mote_t next; // for lists

  // next knock state
  util_chunks_t chunks; // actual chunk encoding for r/w frame buffers
  epoch_t knock;
  uint32_t kchan; // current channel (< med->chans)
  uint64_t kstart, kstop; // microsecond exact start/stop time
  enum {ERR, READY, DONE} kstate:2; // knock handling

  uint8_t z;
};

mote_t mote_new(link_t link);
mote_t mote_free(mote_t m);

// set best knock win/chan/start/stop
mote_t mote_knock(mote_t m, medium_t medium, uint64_t from);

// individual epoch state data
struct epoch_struct
{
  uint8_t secret[32];
  uint64_t base; // microsecond of window 0 start
  epoch_t next; // for epochs_* list utils
  enum {TX, RX} dir:1;
  enum {RESET, PING, ECHO, PAIR, LINK} type:4;

};

epoch_t epoch_new(uint8_t tx);
epoch_t epoch_free(epoch_t e);

// sync point for given window
epoch_t epoch_base(epoch_t e, uint32_t window, uint64_t at);

// simple array utilities
epoch_t epochs_add(epoch_t es, epoch_t e);
epoch_t epochs_rem(epoch_t es, epoch_t e);
size_t epochs_len(epoch_t es);
epoch_t epochs_free(epoch_t es);

///////////////////
// radio devices are single task responsible for all the epochs in one or more mediums
typedef struct radio_struct
{
  uint8_t id;

  // return energy cost, or 0 if unknown medium, use for pre-validation/estimation
  uint32_t (*energy)(tmesh_t tm, uint8_t medium[6]);

  // initialize/get any medium scheduling time/cost and channels
  medium_t (*get)(tmesh_t tm, uint8_t medium[6]);

  // when a medium isn't used anymore, let the radio free it
  medium_t (*free)(tmesh_t tm, medium_t m);

  // perform this knock right _now_
  mote_t (*knock)(tmesh_t tm, medium_t m, uint8_t dir, uint8_t chan, uint8_t *frame);
  
  // active frame buffer
  uint8_t frame[64];

} *radio_t;

#define RADIOS_MAX 1
extern radio_t radio_devices[]; // all of em

// add/set a new device
radio_t radio_device(radio_t device);

// return the next hard-scheduled mote for this radio
mote_t radio_next(radio_t device, tmesh_t tm);

// signal once a frame has been sent/received for this mote
void radio_done(radio_t device, tmesh_t tm, mote_t m);

// validate medium by checking energy
uint32_t radio_energy(tmesh_t tm, uint8_t medium[6]);

// get the full medium
medium_t radio_medium(tmesh_t tm, uint8_t medium[6]);



#endif
