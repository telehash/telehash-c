#ifndef util_frames_h
#define util_frames_h

#include <stdint.h>
#include "lob.h"

// for list of incoming frames
typedef struct util_frame_struct
{
  struct util_frame_struct *prev;
  uint32_t hash;
  uint8_t data[];
} *util_frame_t;

typedef struct util_frames_struct
{

  lob_t inbox; // received packets waiting to be processed
  lob_t outbox; // current packet being sent out

  util_frame_t cache; // stacked linked list of incoming frames in progress

  util_frame_t sending;
  util_frame_t receiving;

  uint16_t size; // frame size
  uint8_t flush:1; // bool to signal a flush is needed
  uint8_t err:1; // unrecoverable failure

} *util_frames_t;


// size of each frame, ideal is for(i=0;i<=1024;i++) if((i-4)%5 == 0 && (i%4)==0) console.log(i);
util_frames_t util_frames_new(uint16_t size);

util_frames_t util_frames_free(util_frames_t frames);

// turn this packet into frames and append, free's out
util_frames_t util_frames_send(util_frames_t frames, lob_t out);

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames);

// total bytes in the inbox/outbox
size_t util_frames_inlen(util_frames_t frames);
size_t util_frames_outlen(util_frames_t frames);

// meta space is (size - 14) and only filled when receiving a meta frame
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data, uint8_t *meta); // data=NULL is ready check

// fills data with the next frame, if no payload available always fills w/ meta frame, safe to re-run (idempotent)
util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data, uint8_t *meta); // data=NULL is ready-check

// this must be called immediately (no inbox interleaved) after last outbox is actually sent to advance payload or clear flush request, returns if more to send
util_frames_t util_frames_sent(util_frames_t frames);

// is just a check to see if there's data waiting to be sent
util_frames_t util_frames_waiting(util_frames_t frames);

// is there an expectation of an incoming frame
util_frames_t util_frames_await(util_frames_t frames);

// are we waiting to send/receive a frame (both waiting && await)
util_frames_t util_frames_busy(util_frames_t frames);

// is a frame pending to be sent immediately
util_frames_t util_frames_pending(util_frames_t frames);

// check error state and clearing it
util_frames_t util_frames_ok(util_frames_t frames);
util_frames_t util_frames_clear(util_frames_t frames);

#endif
