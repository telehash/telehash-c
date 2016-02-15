#ifndef util_frames_h
#define util_frames_h

#include <stdint.h>
#include "lob.h"

// for list of incoming frames
typedef struct util_frame_struct
{
  struct util_frame_struct *prev;
  uint8_t data[];
} *util_frame_t;

typedef struct util_frames_struct
{

  lob_t inbox; // received packets waiting to be processed
  lob_t outbox; // current packet being sent out

  util_frame_t cache; // stacked linked list of incoming frames in progress

  uint32_t inlast; // last good incoming frame hash
  uint32_t outbase; // last sent outbox frame hash

  uint8_t in; // number of incoming frames received/waiting
  uint8_t out; //  number of outgoing frames of outbox sent since outbase

  uint8_t size; // frame size
  uint8_t flush:1; // bool to signal a flush is needed
  uint8_t err:1; // unrecoverable failure

} *util_frames_t;


// size of each frame, min 16 max 128, multiple of 4
util_frames_t util_frames_new(uint8_t size);

util_frames_t util_frames_free(util_frames_t frames);

// turn this packet into frames and append, free's out
util_frames_t util_frames_send(util_frames_t frames, lob_t out);

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames);

// total bytes in the inbox/outbox
size_t util_frames_inlen(util_frames_t frames);
size_t util_frames_outlen(util_frames_t frames);

// the next frame of data in/out, if data NULL return is just ready check bool
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data);
util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data);

// is there an expectation of an incoming frame
util_frames_t util_frames_await(util_frames_t frames);

#endif
