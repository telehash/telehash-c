#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"

typedef struct tmesh_struct *tmesh_t;
typedef struct cmnty_struct *cmnty_t; // list of motes
typedef struct mote_struct *mote_t; // secret, nonce, time, knock
typedef struct medium_struct *medium_t; // channels, energy
typedef struct radio_struct *radio_t; // driver utils
typedef struct knock_struct *knock_t; // single action

// medium management w/ device driver
struct medium_struct
{
  void *arg; // for use by radio device driver
  uint32_t min, max; // cycles to knock, set by driver
  uint32_t avg; // average actual cycles to tx for drift calc
  uint8_t bin[5];
  uint8_t chans; // number of total channels, set by driver
  uint8_t z; // default
  uint8_t radio:4; // radio device id based on radio_devices[]
  uint8_t zshift:4; // exponent offset for this medium
};

// get the full medium
medium_t medium_get(tmesh_t tm, uint8_t medium[5]);

// just convenience util
uint8_t medium_public(medium_t m);


// community management
struct cmnty_struct
{
  tmesh_t tm;
  char *name;
  medium_t medium;
  mote_t public; // ping mote
  mote_t motes; 
  pipe_t pipe; // one pipe per community as it's shared performance
  struct cmnty_struct *next;
  uint8_t max;
};

// maximum neighbors tracked per community
#define NEIGHBORS_MAX 8

// join a new private/public community
cmnty_t tmesh_join(tmesh_t tm, char *medium, char *name);

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t c);

// add a link known to be in this community to look for
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link);

// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  cmnty_t coms;
  lob_t pubim;
  uint32_t last; // last seen cycles for rebasing
  uint32_t epoch; // for relative time into mesh_process
  uint32_t cycles; // remainder for epoch
  knock_t (*sort)(knock_t a, knock_t b);
  uint8_t z; // our preferred z-index
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options);
void tmesh_free(tmesh_t tm);

// process any knock that has been completed by a driver
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k);

// process everything based on current cycle count, optional rebase cycles
tmesh_t tmesh_process(tmesh_t tm, uint32_t at, uint32_t rebase);

// a single knock request ready to go
struct knock_struct
{
  mote_t mote;
  medium_t medium;
  uint32_t start, stop;
  uint32_t done; // actual stop time, done-start is time it took
  uint8_t frame[64];
  uint8_t nonce[8]; // nonce for this knock
  uint8_t chan; // current channel (< med->chans)
  // boolean flags for state tracking
  uint8_t tx:1; // tells radio to tx or rx
  uint8_t ready:1; // is ready to transceive
  uint8_t err:1; // failed
};

// mote state tracking
struct mote_struct
{
  cmnty_t com;
  link_t link; // when known
  mote_t next; // for lists
  uint8_t secret[32];
  uint8_t nonce[8];
  uint8_t chan[2];
  uint32_t at; // cycles until next knock
  util_chunks_t chunks; // actual chunk encoding for r/w frame buffers
  uint16_t sent, received;
  uint8_t z;
  uint8_t order:1; // is hashname compare
  uint8_t ping:1; // is in ping mode
  uint8_t pong:1; // ready for pong
  uint8_t public:1; // is a special public beacon mote
  uint8_t skip:1; // internal flag to skip next advancing
  uint8_t priority:3; // next knock priority
};

mote_t mote_new(link_t link);
mote_t mote_free(mote_t m);

// resets secret/nonce and to ping mode
mote_t mote_reset(mote_t m);

// advance mote ahead next window
mote_t mote_advance(mote_t m);

// least significant nonce bit sets direction
uint8_t mote_tx(mote_t m);

// next knock init
mote_t mote_knock(mote_t m, knock_t k);

// initiates handshake over this synchronized mote
mote_t mote_synced(mote_t m);

// for tmesh sorting
knock_t knock_sooner(knock_t a, knock_t b);

///////////////////
// radio devices are responsible for all mediums
struct radio_struct
{
  // initialize/get any medium scheduling time/cost and channels
  medium_t (*get)(radio_t self, tmesh_t tm, uint8_t medium[5]);

  // when a medium isn't used anymore, let the radio free it
  medium_t (*free)(radio_t self, tmesh_t tm, medium_t m);

  // called whenever a new knock is ready to be scheduled
  tmesh_t (*ready)(radio_t self, tmesh_t tm, knock_t knock);
  
  // shared knock between tmesh and driver
  knock_t knock;

  // for use by the radio driver
  void *arg;

  // guid
  uint8_t id:4;
};

#ifndef RADIOS_MAX
#define RADIOS_MAX 1
#endif
extern radio_t radio_devices[]; // all of em

// globally add/set a new device
radio_t radio_device(radio_t device);



#endif
