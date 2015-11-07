#ifndef chan_h
#define chan_h
#include <stdint.h>
#include "mesh.h"

enum chan_states { CHAN_ENDED, CHAN_OPENING, CHAN_OPEN };

// standalone channel packet management, buffering and ordering
// internal only structure, always use accessors
typedef struct chan_struct
{
  uint32_t id; // wire id (not unique)
  char c[12]; // str of id
  char uid[9]; // process hex id (unique)
  char *type;
  lob_t open; // cached for convenience
  enum chan_states state;
  uint32_t capacity, max; // totals for windowing

  // timer stuff
  uint32_t tsent, trecv; // last send, recv at
  uint32_t timeout; // when in the future to trigger timeout
  
  // reliable tracking
  lob_t out, sent;
  lob_t in;
  uint32_t seq, ack, acked, window;
} *chan_t;

// caller must manage lists of channels per e3x_exchange based on cid
chan_t chan_new(lob_t open); // open must be chan_receive or chan_send next yet
void chan_free(chan_t c);

// sets when in the future this channel should timeout auto-error from no receive, returns current timeout
uint32_t chan_timeout(chan_t c, uint32_t at);

// sets the max size (in bytes) of all buffered data in or out, returns current usage, can pass 0 just to check
uint32_t chan_size(chan_t c, uint32_t max); // will actively signal incoming window size depending on capacity left

// incoming packets
uint8_t chan_receive(chan_t c, lob_t inner, uint32_t now); // ret if accepted/valid into receiving queue, call w/ no inner to trigger timeouts
void chan_sync(chan_t c, uint8_t sync); // false to force start timeouts (after any new handshake), true to cancel and resend last packet (after any e3x_exchange_sync)
lob_t chan_receiving(chan_t c); // get next avail packet in order, null if nothing

// outgoing packets
lob_t chan_oob(chan_t c); // id/ack/miss only headers base packet
lob_t chan_packet(chan_t c);  // creates a sequenced packet w/ all necessary headers, just a convenience
uint8_t chan_send(chan_t c, lob_t inner); // adds to sending queue
lob_t chan_sending(chan_t c, uint32_t now); // must be called after every send or receive, pass pkt to e3x_exchange_encrypt before sending

// convenience functions
char *chan_uid(chan_t c); // process-unique string id
uint32_t chan_id(chan_t c); // numeric of the open->c id
char *chan_c(chan_t c); // string of the c id
lob_t chan_open(chan_t c); // returns the open packet (always cached)

enum chan_states chan_state(chan_t c);



#endif
