#ifndef chunks_h
#define chunks_h

#include <stdint.h>
#include "lob.h"

typedef struct chunks_struct
{
  uint8_t size;
  uint32_t window;
  uint8_t *writing, *write;
  uint32_t writelen;
  uint8_t *reading, *read;
  uint32_t readlen;
} *chunks_t;

// size of each chunk, how many chunks to send before getting acks
chunks_t chunks_new(uint8_t size, uint32_t window);

chunks_t chunks_free(chunks_t chunks);

// turn this packet into chunks
chunks_t chunks_send(chunks_t chunks, lob_t out);

// get any packets that have been reassembled from incoming chunks
lob_t chunks_receive(chunks_t chunks);

// these are for sending on a transport that is message/frame based

// get the next chunk, put its length in len
uint8_t *chunks_next(chunks_t chunks, uint8_t *len);

// >0 ack that a chunk was sent, <0 drop all the chunks in the current packet, 0 start-over/resend current packet
chunks_t chunks_ack(chunks_t chunks, int ack);

// process an incoming individual chunk, also parses as a raw lob as a fallback
chunks_t chunks_chunk(chunks_t chunks, uint8_t *chunk, uint8_t len);

// these are for a stream-based transport

// how many bytes are there ready to be sent
uint32_t chunks_available(chunks_t chunks);

// return the next block of data to be written to the stream transport
uint8_t *chunks_write(chunks_t chunks, uint32_t *len);

// advance the write this far
chunks_t chunks_written(chunks_t chunks, uint32_t len);

// process incoming stream data into any packets, auto-generates acks that show up in _write()
chunks_t chunks_read(chunks_t chunks, uint8_t *block, uint32_t len);

#endif
