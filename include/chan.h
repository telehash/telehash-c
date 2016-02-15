#ifndef chan_h
#define chan_h
#include <stdint.h>
#include "mesh.h"

enum chan_states { CHAN_ENDED, CHAN_OPENING, CHAN_OPEN };

// standalone channel packet management, buffering and ordering
// internal only structure, always use accessors
struct chan_struct
{
  link_t link; // so channels can be first-class
  chan_t next; // links keep lists
  uint32_t id; // wire id (not unique)
  char *type;
  lob_t open; // cached for convenience
  uint32_t capacity, max; // totals for windowing

  // timer stuff
  uint32_t tsent, trecv; // last send, recv at
  uint32_t timeout; // when in the future to trigger timeout
  
  // reliable tracking
  lob_t sent;
  lob_t in;
  uint32_t seq, ack, acked, window;
  
  // direct handler
  void *arg;
  void (*handle)(chan_t c, void *arg);

  enum chan_states state;
};

// caller must manage lists of channels per e3x_exchange based on cid
chan_t chan_new(lob_t open); // open must be chan_receive or chan_send next yet
chan_t chan_free(chan_t c);

// sets when in the future this channel should timeout auto-error from no receive, returns current timeout
uint32_t chan_timeout(chan_t c, uint32_t at);

// sets the max size (in bytes) of all buffered data in or out, returns current usage, can pass 0 just to check
uint32_t chan_size(chan_t c, uint32_t max); // will actively signal incoming window size depending on capacity left

// incoming packets
chan_t chan_receive(chan_t c, lob_t inner); // process into receiving queue
chan_t chan_sync(chan_t c, uint8_t sync); // false to force start timeouts (after any new handshake), true to cancel and resend last packet (after any e3x_exchange_sync)
lob_t chan_receiving(chan_t c); // get next avail packet in order, null if nothing

// outgoing packets
lob_t chan_oob(chan_t c); // id/ack/miss only headers base packet
lob_t chan_packet(chan_t c);  // creates a sequenced packet w/ all necessary headers, just a convenience
chan_t chan_send(chan_t c, lob_t inner); // encrypts and sends packet out link
chan_t chan_err(chan_t c, char *err); // generates local-only error packet for next chan_process()

// must be called after every send or receive, processes resends/timeouts, fires handlers
chan_t chan_process(chan_t c, uint32_t now);

// set up internal handler for all incoming packets on this channel
chan_t chan_handle(chan_t c, void (*handle)(chan_t c, void *arg), void *arg);

// convenience functions, accessors
chan_t chan_next(chan_t c); // c->next
uint32_t chan_id(chan_t c); // c->id
lob_t chan_open(chan_t c); // c->open
enum chan_states chan_state(chan_t c);



#endif
