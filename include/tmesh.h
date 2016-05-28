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

beacon > lost
  * tm->beacon alongside tm->signal
  * beacon is a stream, always starts w/ meta frame
    * meta contains sender nickname to ignore once linked (just for now, future is random id)
  * when no motes, beacon uses fast medium, if any other mote it goes slow
  * signal is off or slow when beaconing
  * only beacon open rx seeks
  * when mote looses signal it drops entirely, beacon goes fast
    * if stream request is not accepted in X signals, also drop
  * streams just go idle (not lost), low priori rx skip tx
  * after beacon stream exchanges handshake
    * start our signal to include in stream
    * create mote
    * move beacon to mote as stream
    * reset beacon after link is up
    * create their signal once received in stream meta
  * when routed packet is requested on a stream
    * cache from who on outgoing stream
    * include orig sender's full blocks in stream meta
    * faster way to establish signal to neighbor than waiting for slow beacon

... more notes
  * beacon (signal) advertises its shared stream
  * shared stream uses beacon id in secret to be unique
  * all tempo keys roll community name + other stuff + password

SECURITY TODOs
  * generate a temp hashname on boot, use that to do all beacon and shared stream handling

NEXT
  * use ad-hoc beacons for faster signal stream request/accept

*/

typedef struct tmesh_struct *tmesh_t; // joined community motes/signals
typedef struct mote_struct *mote_t; // local link info, signal and list of stream tempos
typedef struct tempo_struct *tempo_t; // single tempo, is a signal or stream
typedef struct knock_struct *knock_t; // single txrx action

// overall tmesh manager
struct tmesh_struct
{
  mesh_t mesh;
  uint32_t at; // last known
  
  // community deets
  char *community;
  char *password; // optional
  mote_t motes;
  tempo_t signal; // outgoing signal, unique to us
  tempo_t stream; // have an always-running shared stream, keyed from beacon for handshakes, RX for alerts
  tempo_t beacon; // only one of these, advertises our shared stream
  uint32_t app; // available for app to use to send custom block

  // driver interface
  tempo_t (*sort)(tmesh_t tm, tempo_t a, tempo_t b);
  tmesh_t (*schedule)(tmesh_t tm); // called whenever a new knock is ready to be scheduled
  tmesh_t (*advance)(tmesh_t tm, tempo_t tempo, uint8_t seed[8]); // advances tempo to next window
  tmesh_t (*medium)(tmesh_t tm, tempo_t tempo, uint8_t seed[8], uint32_t medium); // driver can initialize/update a tempo's medium
  tmesh_t (*accept)(tmesh_t tm, hashname_t id, uint32_t app); // driver handles new neighbors, returns tm to continue or NULL to ignore
  tmesh_t (*free)(tmesh_t tm, tempo_t tempo); // driver can free any associated tempo resources
  knock_t knock;
};

// join a new tmesh community, pass optional
tmesh_t tmesh_new(mesh_t mesh, char *name, char *pass);
tmesh_t tmesh_free(tmesh_t tm);

// process knock that has been completed by a driver
// returns a stream if there's new packets available
tempo_t tmesh_knocked(tmesh_t tm);

// at based on current cycle count since start or last rebase
tmesh_t tmesh_schedule(tmesh_t tm, uint32_t at);

// call before a schedule to rebase (subtract) given cycles off all at's (to prevent overflow)
tmesh_t tmesh_rebase(tmesh_t tm, uint32_t at);

// returns mote for this link, creating one if a stream is provided
mote_t tmesh_mote(tmesh_t tm, link_t link);

// drops and free's this mote (link just goes to down state if no other paths)
tmesh_t tmesh_demote(tmesh_t tm, mote_t mote);

// returns mote for this id if one exists
mote_t tmesh_moted(tmesh_t tm, hashname_t id);

// update/signal our current app id
tmesh_t tmesh_appid(tmesh_t tm, uint32_t id);

// tempo state
struct tempo_struct
{
  tmesh_t tm; // mostly convenience
  mote_t mote; // parent mote (except for our outgoing signal) 
  void *driver; // for driver use, set during tm->tempo()
  util_frames_t frames; // r/w frame buffers for streams
  uint32_t qos_remote, qos_local; // last qos from/about this tempo
  uint32_t medium; // id
  uint32_t at; // cycles until next knock in current window
  uint32_t seq; // window increment part of nonce
  uint16_t c_tx, c_rx; // current counts
  uint16_t c_bad; // dropped bad frames
  int16_t last, best, worst; // rssi
  uint8_t secret[32];
  uint8_t c_miss, c_skip, c_idle; // how many of the last rx windows were missed (expected), skipped (scheduling), or idle
  uint8_t chan; // channel of next knock
  uint8_t priority; // next knock priority
  union
  {
    struct
    {
      uint8_t is_signal:1;
      uint8_t unused1:1;
      uint8_t qos_request:1;
      uint8_t qos_accept:1;
    };
    struct
    {
      uint8_t unused2:1;
      uint8_t is_stream:1;
      uint8_t requesting:1;
      uint8_t accepting:1;
      uint8_t direction:1; // 1==TX, 0==RX
    };
  } state;
};

// a single convenient knock request ready to go
struct knock_struct
{
  tempo_t tempo;
  uint32_t seekto; // when is the next knock if seeking
  uint32_t started, stopped; // actual times
  int16_t rssi, snr; // set by driver only after rx
  uint8_t frame[64];
  uint8_t nonce[8]; // convenience
  // boolean flags for state tracking, etc
  uint8_t is_active:1; // is actively transceiving
  uint8_t is_tx:1; // current window direction (copied from tempo for convenience)
  uint8_t do_err:1; // driver sets if failed
  uint8_t do_gone:1; // driver sets to fail the signal/stream
};

// mote state tracking
struct mote_struct
{
  mote_t next; // for lists
  mote_t via; // router mote
  tmesh_t tm;
  link_t link;
  tempo_t signal; // tracks their signal
  tempo_t stream; // is a private stream, optionally can track their shared stream (TODO)
  uint32_t app; // most recent app block from them
};

// return current mote appid
uint32_t mote_appid(mote_t mote);

#endif
