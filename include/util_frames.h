#ifndef util_frames_h
#define util_frames_h

#include <stdint.h>
#include "lob.h"

// for list of incoming frames
typedef struct util_frame_struct
{
  struct util_frame_struct *prev;
  uint8_t size;
  uint8_t data[];
} *util_frame_t;

typedef struct util_frames_struct
{

  util_frame_t reading; // stacked linked list of incoming frames

  lob_t writing;
  size_t writeat; // offset into lob_raw()
  uint16_t waitat; // gets to 256, offset into current frame
  uint8_t waiting; // current writing frame size;
  uint8_t readat; // always less than a max frame, offset into reading

  uint8_t cap;
  uint8_t blocked:1, blocking:1, ack:1, err:1; // bool flags
} *util_frames_t;


// size of each frame, 0 == MAX (256)
util_frames_t util_frames_new(uint8_t size);

util_frames_t util_frames_free(util_frames_t frames);

// turn this packet into frames and append, free's out
util_frames_t util_frames_send(util_frames_t frames, lob_t out);

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames);

// bytes waiting to be sent
uint32_t util_frames_writing(util_frames_t frames);


////// these are for a stream-based transport

// how many bytes are there ready to write
uint32_t util_frames_len(util_frames_t frames);

// return the next block of data to be written to the stream transport, max len is util_frames_len()
uint8_t *util_frames_write(util_frames_t frames);

// advance the write this far, don't mix with util_frames_out() usage
util_frames_t util_frames_written(util_frames_t frames, size_t len);

// queues incoming stream based data
util_frames_t util_frames_read(util_frames_t frames, uint8_t *block, size_t len);

////// these are for frame-based transport

// size of the next frame, -1 when none, max is frames size-1
int16_t util_frames_size(util_frames_t frames);

// return the next frame of data, use util_frames_next to advance
uint8_t *util_frames_frame(util_frames_t frames);

// peek into what the next frame size will be, to see terminator ones
int16_t util_frames_peek(util_frames_t frames);

// process incoming frame
util_frames_t util_frames_data(util_frames_t frames, uint8_t *frame, int16_t size);

// advance the write past the current frame
util_frames_t util_frames_next(util_frames_t frames);


#endif
