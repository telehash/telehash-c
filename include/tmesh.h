#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"
#include <stdbool.h>

/*

tempo_t is a synchronized comm thread
every tempo has a medium id that identifies the transceiver details
all tempos belong to a mote->community for grouping
two types of tempos: signals and streams
motes have one constant signal tempo and on-demand stream tempos
signal tempo
  - use shared secrets across the community
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
  - tm->tempo(tempo), sets window min/max, can change secret

stream tempos are lost until first rx
lost streams are reset each signal
lost signal hash is 8+50+4 w/ hash of just 50, rx can detect difference
!signal->lost after first good rx
must be commanded to change tx signal->lost, scheduled along w/ medium change
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
  // driver interface
  tempo_t (*sort)(tmesh_t tm, tempo_t a, tempo_t b);
  tmesh_t (*notify)(tmesh_t tm, lob_t notice); // just used for seq overflow increment notifications right now
  tmesh_t (*schedule)(tmesh_t tm, knock_t knock); // called whenever a new knock is ready to be scheduled
  tempo_t (*advance)(tmesh_t tm, tempo_t tempo, uint8_t seed[8]); // advances tempo to next window
  tmesh_t (*init)(tmesh_t tm, tempo_t tempo, cmnty_t com); // driver can initialize a new tempo/community
  tmesh_t (*free)(tmesh_t tm, tempo_t tempo, cmnty_t com); // driver can free any associated resources
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
  knock_t knock, seek; // managed by radio driver
  uint32_t m_lost, m_signal, m_stream; // default mediums
  uint16_t seq; // increment every reboot or overflow
};

// join a new community, starts lost signal on given medium
cmnty_t tmesh_join(tmesh_t tm, char *name, uint32_t mediums[3]);

// leave any community
tmesh_t tmesh_leave(tmesh_t tm, cmnty_t com);

// start looking for this link in this community
mote_t tmesh_find(tmesh_t tm, cmnty_t com, link_t link, uint32_t mediums[3]);

// return the first mote found for this link in any community
mote_t tmesh_mote(tmesh_t tm, link_t link);

// tempo state
struct tempo_struct
{
  tempo_t next; // for lists
  mote_t mote; // ownership
  cmnty_t com; // needed when no mote (our signal)
  void *driver; // for driver use, set during tm->tempo()
  util_frames_t frames; // r/w frame buffers for streams
  uint32_t medium; // id
  uint32_t at; // cycles until next knock in current window
  uint16_t itx, irx; // current counts
  uint16_t bad; // dropped bad frames
  uint16_t seq; // local part of nonce
  uint8_t secret[32];
  uint8_t miss, skip; // how many of the last rx windows were missed (nothing received) or skipped (scheduling)
  uint8_t chan; // channel of next knock
  uint8_t last, best, worst; // rssi
  uint8_t signal:1; // type of tempo
  uint8_t tx:1; // current window direction
  uint8_t lost:1; // if currently lost
  uint8_t priority:4; // next knock priority
};

// a single knock request ready to go
struct knock_struct
{
  tempo_t tempo;
  tempo_t sync; // if there's another tempo to sync after
  uint32_t stopped; // actual stop time
  uint8_t frame[64];
  uint8_t nonce[8]; // convenience
  uint8_t rssi; // set by driver only after rx
  // boolean flags for state tracking, etc
  uint8_t ready:1; // is ready to transceive
  uint8_t err:1; // failed
};

// mote state tracking
struct mote_struct
{
  cmnty_t com;
  pipe_t pipe; // one pipe per mote to start/find best stream
  link_t link;
  mote_t next; // for lists
  tempo_t signal;
  tempo_t streams;
  lob_t cached; // queued packet waiting for stream
  uint16_t seq; // helps detect resets, part of the nonce
};

#endif
