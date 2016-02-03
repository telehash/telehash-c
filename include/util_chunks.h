#ifndef util_chunks_h
#define util_chunks_h

#include <stdint.h>
#include "lob.h"

// for list of incoming chunks
typedef struct util_chunk_struct
{
  struct util_chunk_struct *prev;
  uint8_t size;
  uint8_t data[];
} *util_chunk_t;

typedef struct util_chunks_struct
{

  util_chunk_t reading; // stacked linked list of incoming chunks

  lob_t writing;
  size_t writeat; // offset into lob_raw()
  uint16_t waitat; // gets to 256, offset into current chunk
  uint8_t waiting; // current writing chunk size;
  uint8_t readat; // always less than a max chunk, offset into reading

  uint8_t cap;
  uint8_t blocked:1, blocking:1, ack:1, err:1; // bool flags
} *util_chunks_t;


// size of each chunk, 0 == MAX (256)
util_chunks_t util_chunks_new(uint8_t size);

util_chunks_t util_chunks_free(util_chunks_t chunks);

// turn this packet into chunks and append, free's out
util_chunks_t util_chunks_send(util_chunks_t chunks, lob_t out);

// get any packets that have been reassembled from incoming chunks
lob_t util_chunks_receive(util_chunks_t chunks);

// bytes waiting to be sent
uint32_t util_chunks_writing(util_chunks_t chunks);


////// these are for a stream-based transport

// how many bytes are there ready to write
uint32_t util_chunks_len(util_chunks_t chunks);

// return the next block of data to be written to the stream transport, max len is util_chunks_len()
uint8_t *util_chunks_write(util_chunks_t chunks);

// advance the write this far, don't mix with util_chunks_out() usage
util_chunks_t util_chunks_written(util_chunks_t chunks, size_t len);

// queues incoming stream based data
util_chunks_t util_chunks_read(util_chunks_t chunks, uint8_t *block, size_t len);

////// these are for frame-based transport

// size of the next chunk, -1 when none, max is chunks size-1
int16_t util_chunks_size(util_chunks_t chunks);

// return the next chunk of data, use util_chunks_next to advance
uint8_t *util_chunks_frame(util_chunks_t chunks);

// peek into what the next chunk size will be, to see terminator ones
int16_t util_chunks_peek(util_chunks_t chunks);

// process incoming chunk
util_chunks_t util_chunks_chunk(util_chunks_t chunks, uint8_t *chunk, int16_t size);

// advance the write past the current chunk
util_chunks_t util_chunks_next(util_chunks_t chunks);


#endif
