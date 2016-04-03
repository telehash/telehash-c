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

stream hold/lost/idle states
  * hold means don't schedule
  * idle means don't signal, dont schedule TX but do schedule RX (if !hold)
  * lost means signal (if !idle), hold = request, !hold || mote->signal->lost = accept
    - TX/RX signal accept is stream resync event
  - idle is cleared on any new send data, set after any TX/RX if !ready && !await
  - mote_gone() called by the driver based on missed RX, sets lost if !idle, hold if idle

TODO, move flags to explicit vs implicit:
  * do_schedule and !do_schedule
    - skip TX if !ready && !awaiting
    - set true on any new data
  * do_signal and !do_signal
    - do_schedule = true if mote->signal->lost
    - request if !do_schedule, else accept
  * mote_gone() called by the driver based on missed RX
    - sets do_schedule = false
    - sets do_signal = true if ready||awaiting


*/

typedef struct tmesh_struct *tmesh_t; // joined community motes/signals
typedef struct mote_struct *mote_t; // local link info, signal and list of stream tempos
typedef struct tempo_struct *tempo_t; // single tempo, is a signal or stream
typedef struct knock_struct *knock_t; // single txrx action

// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  
  // community deets
  char *community;
  mote_t motes;
  tempo_t signal; 
  uint32_t m_lost, m_signal, m_stream; // default mediums

  // driver interface
  tempo_t (*sort)(tmesh_t tm, tempo_t a, tempo_t b);
  tmesh_t (*schedule)(tmesh_t tm); // called whenever a new knock is ready to be scheduled
  tmesh_t (*advance)(tmesh_t tm, tempo_t tempo, uint8_t seed[8]); // advances tempo to next window
  tmesh_t (*init)(tmesh_t tm, tempo_t tempo); // driver can initialize a new tempo
  tmesh_t (*free)(tmesh_t tm, tempo_t tempo); // driver can free any associated tempo resources
  knock_t knock;
  
  // config/tuning flags
  uint8_t discoverable:1;

};

// join a new tmesh community
tmesh_t tmesh_new(mesh_t mesh, char *name, uint32_t mediums[3]);
tmesh_t tmesh_free(tmesh_t tm);

// start/reset our outgoing signal
tempo_t tmesh_signal(tmesh_t tm, uint32_t seq, uint32_t medium);

// process knock that has been completed by a driver
tmesh_t tmesh_knocked(tmesh_t tm);

//  based on current cycle count, optional rebase cycles
tmesh_t tmesh_schedule(tmesh_t tm, uint32_t at, uint32_t rebase);

// start looking for this link in this community
mote_t tmesh_find(tmesh_t tm, link_t link, uint32_t m_lost);

// returns an existing mote for this link (if any)
mote_t tmesh_mote(tmesh_t tm, link_t link);

// tempo state
struct tempo_struct
{
  tmesh_t tm;
  mote_t mote; // ownership
  void *driver; // for driver use, set during tm->tempo()
  util_frames_t frames; // r/w frame buffers for streams
  uint32_t medium; // id
  uint32_t at; // cycles until next knock in current window
  uint32_t seq; // window increment part of nonce
  uint16_t itx, irx; // current counts
  uint16_t bad; // dropped bad frames
  int16_t last, best, worst; // rssi
  uint8_t secret[32];
  uint8_t miss, skip; // how many of the last rx windows were missed (nothing received) or skipped (scheduling)
  uint8_t chan; // channel of next knock
  uint8_t signal:1; // type of tempo
  uint8_t tx:1; // current window direction
  uint8_t lost:1; // if currently lost
  uint8_t hold:1; // on hold, don't schedule (requested streams)
  uint8_t priority:4; // next knock priority
};

// a single knock request ready to go
struct knock_struct
{
  tempo_t tempo;
  uint32_t seekto; // when is the next knock if seeking
  uint32_t started, stopped; // actual times
  int16_t rssi; // set by driver only after rx
  uint8_t frame[64];
  uint8_t nonce[8]; // convenience
  tempo_t syncs[5]; // max number of tempos being sync'd in this knock
  // boolean flags for state tracking, etc
  uint8_t ready:1; // is ready to transceive
  uint8_t err:1; // failed
  uint8_t lost:1; // if is lost format
};

// mote state tracking
struct mote_struct
{
  mote_t next; // for lists
  tmesh_t tm;
  pipe_t pipe; // one pipe per mote to start stream as needed
  link_t link;
  tempo_t signal;
  tempo_t stream;
  uint32_t seen; // first seen (for debugging)
};

#endif
