#ifndef tmesh_h
#define tmesh_h

#include "mesh.h"
#include <stdbool.h>

// how many knocks can be prepared in advance, must be >= 2
#define TMESH_KNOCKS 5

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

*/

typedef struct tmesh_struct *tmesh_t; // joined community motes/signals
typedef struct mote_struct *mote_t; // local link info, signal and list of stream tempos
typedef struct tempo_struct *tempo_t; // single tempo, is a signal or stream

// a single convenient tx/rx action ready to go
typedef struct knock_struct
{
  struct knock_struct *next; // for tm->ready list
  tempo_t tempo;
  uint32_t started, stopped; // actual times
  uint8_t frame[64];
  uint8_t nonce[8]; // convenience
  // boolean flags for state tracking, etc
  uint8_t is_active:1; // is actively transceiving
  uint8_t is_tx:1; // current window direction (copied from tempo for convenience)
  uint8_t do_err:1; // driver sets if failed
} *knock_t;

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
  uint32_t route; // available for app-level routing logic

  // externally event when a tempo is created (has medium) and removed (0 medium)
  tmesh_t (*tempo)(tmesh_t tm, tempo_t tempo, uint8_t seed[8], uint32_t medium);

  // driver handles new neighbors, returns tm to continue or NULL to ignore
  tmesh_t (*accept)(tmesh_t tm, hashname_t id, uint32_t route);

  struct knock_struct knocks[TMESH_KNOCKS]; // 0 is idle slot, 1+ is ready slots
  knock_t ready;
  knock_t idle;
  
  uint8_t seen[5]; // recently seen short hn from a beacon
};

// join a new tmesh community, pass optional
tmesh_t tmesh_new(mesh_t mesh, char *name, char *pass);
tmesh_t tmesh_free(tmesh_t tm);

// process knock that has been completed by a driver
// returns a tempo when successful rx in case there's new packets available
tempo_t tmesh_knocked(tmesh_t tm);

// check all knocks if done, rebuilt tm->ready
tmesh_t tmesh_process(tmesh_t tm);

// call before a _process to rebase (subtract) given cycles off all at's (to prevent overflow)
tmesh_t tmesh_rebase(tmesh_t tm, uint32_t deduct);

// returns mote for this link, creating one if a stream is provided
mote_t tmesh_mote(tmesh_t tm, link_t link);

// drops and free's this mote (link just goes to down state if no other paths)
tmesh_t tmesh_demote(tmesh_t tm, mote_t mote);

// returns mote for this id if one exists
mote_t tmesh_moted(tmesh_t tm, hashname_t id);

// update/signal our current route value
tmesh_t tmesh_route(tmesh_t tm, uint32_t route);

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
  // a byte of state flags for each tempo type
  union
  {
    struct
    {
      uint8_t is_signal:1;
      uint8_t unused1:1;
      uint8_t qos_ping:1;
      uint8_t qos_pong:1;
      uint8_t seen:1; // beacon only
      uint8_t adhoc:1;
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

// mote state tracking
struct mote_struct
{
  mote_t next; // for lists
  mote_t via; // router mote
  tmesh_t tm;
  link_t link;
  tempo_t signal; // tracks their signal
  tempo_t stream; // is a private stream, optionally can track their shared stream (TODO)
  uint32_t route; // most recent route block from them
};

// find a stream to send it to for this mote
mote_t mote_send(mote_t mote, lob_t packet);

// send this packet to this id via this router
mote_t mote_route(mote_t router, hashname_t to, lob_t packet);


#endif
