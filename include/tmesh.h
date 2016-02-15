#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"

/*

one mote per link
every mote is part of one community
public communities start w/ "Public"
private include hashname in secret, new motes must be invited by active ones
every community has one or more mediums
each community medium has 1+ beacon motes
beacon tx/rx is always fixed channel, public community has dedicated beacon hashname \0\0\0\0...
one beacon mote per known hashname for signal detection and handshakes, cloned for new links
all known links in a community have motes bound to just one medium

mote_sync(a, b) - clones state from a to b
  - use this on any incoming public beacon rx to private beacon tx
  - use to transition from beacon to link mote
*/

typedef struct tmesh_struct *tmesh_t;
typedef struct cmnty_struct *cmnty_t; // list of mediums and beacon mote
typedef struct mote_struct *mote_t; // secret, nonce, time, knock, link
typedef struct medium_struct *medium_t; // channels, energy
typedef struct radio_struct *radio_t; // driver utils
typedef struct knock_struct *knock_t; // single action

// medium management w/ device driver
struct medium_struct
{
  cmnty_t com; // mediums belong to one community
  medium_t next; // for lists
  void *arg; // for use by radio device driver
  uint32_t min, max; // cycles to knock, set by driver
  uint32_t avg; // average actual cycles to tx for drift calc
  uint8_t bin[5];
  uint8_t chans; // number of total channels, set by driver
  uint8_t z; // default
  uint8_t radio:4; // radio device id based on radio_devices[]
  uint8_t zshift:4; // window time exponent offset for this medium
};

// community management
struct cmnty_struct
{
  tmesh_t tm;
  char *name;
  medium_t medium; // TODO, support multiple per community
  mote_t beacons;
  mote_t links;
  pipe_t pipe; // one pipe per community as it's shared performance
  struct cmnty_struct *next;
};

// join a new private/public community
cmnty_t tmesh_join(tmesh_t tm, char *medium, char *name);

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t c);

// add a link already known to be in this community
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link);

// start looking for this hashname in this community, will link once found
mote_t tmesh_seek(tmesh_t tm, cmnty_t c, hashname_t id);

// if there's a mote for this link, return it
mote_t tmesh_mote(tmesh_t tm, link_t link);

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
  uint8_t seed[4]; // random seed for restart detection
  uint8_t z; // our current z-index
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
  uint32_t start, stop; // requested start/stop times
  uint32_t started, stopped; // actual start/stop times
  uint8_t frame[64];
  uint8_t nonce[8]; // nonce for this knock
  uint8_t chan; // current channel (< med->chans)
  uint8_t rssi; // set by driver only after rx
  // boolean flags for state tracking, etc
  uint8_t tx:1; // tells radio to tx or rx
  uint8_t ready:1; // is ready to transceive
  uint8_t err:1; // failed
};

// mote state tracking
struct mote_struct
{
  medium_t medium;
  link_t link; // only on link motes
  hashname_t beacon; // only on beacon motes
  mote_t next; // for lists
  util_frames_t frames; // r/w frame buffers
  uint32_t txhash, rxhash, cash; // dup detection
  uint32_t at; // cycles until next knock
  uint16_t txz, rxz; // empty tx/rx counts
  uint16_t txs, rxs; // current tx/rx counts
  uint16_t bad; // dropped bad frames
  uint8_t secret[32];
  uint8_t nonce[8];
  uint8_t seed[4]; // last seen seed to detect resets
  uint8_t chan[2];
  uint8_t last, best, worst; // rssi
  uint8_t z;
  uint8_t order:1; // is hashname compare
  uint8_t public:1; // special public beacon mote
  uint8_t priority:3; // next knock priority
};

// these are primarily for internal use

mote_t mote_new(medium_t medium, hashname_t id);
mote_t mote_free(mote_t m);

// resets secret/nonce and to ping mode
mote_t mote_reset(mote_t m);

// advance mote ahead next window
mote_t mote_advance(mote_t m);

// least significant nonce bit sets direction
uint8_t mote_tx(mote_t m);

// next knock init
mote_t mote_knock(mote_t m, knock_t k);

// initiates handshake over beacon mote
mote_t mote_handshake(mote_t m);

// attempt to establish link from a beacon mote
mote_t mote_link(mote_t m);

// process new link data on a mote
mote_t mote_process(mote_t m);

// for tmesh sorting
knock_t knock_sooner(knock_t a, knock_t b);

///////////////////
// radio devices must process all mediums
struct radio_struct
{
  // initialize any medium scheduling time/cost and channels
  medium_t (*init)(radio_t self, medium_t m);

  // when a medium isn't used anymore, let the radio free any associated resources
  medium_t (*free)(radio_t self, medium_t m);

  // called whenever a new knock is ready to be scheduled
  medium_t (*ready)(radio_t self, medium_t m, knock_t knock);
  
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
