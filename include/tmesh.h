#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"

/*

tempo_t is a synchronized comm thread
every tempo has a medium id that identifies the transceiver details
all tempos belong to a community for grouping
two types of tempos: signals and streams
motes have one constant signal tempo and on-demand stream tempos
signal tempo
  - use shared secrets
  - can be lost signal, derived secret and no neighbors
  - signal tempo should use limited medium to ease syncing
  - include neighbor info and stream requests
  - either side initiates stream tempo
  - 6*10-block format, +4 mmh
    - neighbors (+rssi)
    - stream req (+repeat)
    - signals (+hops)
stream tempo
  - use one-time secrets
  - initiated from signals
  - frames only
  - flush meta space for app use
  - inside channel sent link signal sync

mediums
  - limit # of channels
  - set rx tolerance
  - indicate power/lna for rssi comparisons

*/

typedef struct tmesh_struct *tmesh_t; // list of communities
typedef struct cmnty_struct *cmnty_t; // list of motes, our own signal tempo
typedef struct mote_struct *mote_t; // local link info, signal and list of stream tempos
typedef struct tempo_struct *tempo_t; // single tempo, is a signal or stream
typedef struct knock_struct *knock_t; // single txrx action

// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  cmnty_t coms;
  lob_t pubim;
  uint32_t last; // last seen cycles for rebasing
  knock_t (*sort)(tmesh_t tm, knock_t a, knock_t b);
  tmesh_t (*notify)(tmesh_t tm, lob_t notice); // just used for seq overflow increment notifications right now
  tmesh_t (*schedule)(tmesh_t tm, knock_t knock); // called whenever a new knock is ready to be scheduled
  uint16_t seq; // increment every reboot or overflow
};

// create a new tmesh radio network bound to this mesh
tmesh_t tmesh_new(mesh_t mesh, lob_t options);
void tmesh_free(tmesh_t tm);

// process any knock that has been completed by a driver
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k);

//  based on current cycle count, optional rebase cycles
tmesh_t tmesh_schedule(tmesh_t tm, uint32_t at, uint32_t rebase);

// community management
struct cmnty_struct
{
  tmesh_t tm;
  char *name;
  mote_t motes;
  tempo_t signal; 
  struct cmnty_struct *next;
  knock_t knock; // managed by radio driver
};

// join a new community, start lost signal on given medium
cmnty_t tmesh_join(tmesh_t tm, char *name, char *medium);

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t c);

// start looking for this link in this community
mote_t tmesh_find(tmesh_t tm, cmnty_t c, link_t link);

// return the first mote found for this link in any community
mote_t tmesh_mote(tmesh_t tm, link_t link);

// tempo state
struct tempo_struct
{
  tempo_t next; // for lists
  mote_t mote; // ownership
  union {
    util_frames_t frames; // r/w frame buffers for streams
    void *reserved; // for signals
  };
  uint32_t at; // cycles until next knock
  uint16_t tx, rx, miss; // current tx/rx counts
  uint16_t bad; // dropped bad frames
  uint16_t seq; // local part of nonce
  uint8_t secret[32];
  uint8_t medium[4];
  uint8_t last, best, worst; // rssi
  uint8_t order:1; // is hashname compare
  uint8_t signal:1; // type of tempo
  uint8_t priority:6; // next knock priority
};

// a single knock request ready to go
struct knock_struct
{
  mote_t mote;
  uint32_t start, stop; // requested start/stop times
  uint32_t started, stopped; // actual start/stop times
  uint8_t frame[64];
  uint8_t nonce[8]; // nonce for this knock
  uint8_t chan; // current channel
  uint8_t rssi; // set by driver only after rx
  // boolean flags for state tracking, etc
  uint8_t tx:1; // tells radio to tx or rx
  uint8_t ready:1; // is ready to transceive
  uint8_t err:1; // failed
};

// mote state tracking
struct mote_struct
{
  pipe_t pipe; // one pipe per community as it's shared performance
  link_t link;
  mote_t next; // for lists
  tempo_t signal;
  tempo_t streams;
  uint16_t seq; // helps detect resets
};

// these are primarily for internal use

tempo_t tempo_new(mote_t m, bool signal);
tempo_t tempo_free(tempo_t t);

// advance mote ahead next window
tempo_t tempo_advance(tempo_t t);

// least significant nonce bit sets direction
uint8_t tempo_tx(tempo_t t);

// next knock init
tempo_t tempo_knock(tempo_t t, knock_t k);

// process new stream data
tempo_t tempo_process(tempo_t t);

// for tmesh sorting
knock_t knock_sooner(tmesh_t tm, knock_t a, knock_t b);

#endif
