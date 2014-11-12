#ifndef chunks_h
#define chunks_h

#include <stdint.h>
#include "lob.h"

typedef struct chunks_struct *chunks_t;

// max size of each chunk (>1), how many chunks to send before getting acks (>0)
chunks_t chunks_new(uint8_t size, uint32_t window);

chunks_t chunks_free(chunks_t chunks);

// turn this packet into chunks
chunks_t chunks_send(chunks_t chunks, lob_t out);

// get any packets that have been reassembled from incoming chunks
lob_t chunks_receive(chunks_t chunks);

// just a utility to see how many bytes are waiting to be sent in total
uint32_t chunks_waiting(chunks_t chunks);

////// these are for sending on a transport that is message/frame based

// get the next chunk, put its length in len, advances
uint8_t *chunks_next(chunks_t chunks, uint8_t *len);

// >0 ack that a chunk was sent, <0 drop all the packets partially written, 0 start-over/resend current window
chunks_t chunks_ack(chunks_t chunks, int ack);

// process an incoming individual chunk, also parses as a raw lob as a fallback
chunks_t chunks_chunk(chunks_t chunks, uint8_t *chunk, uint8_t len);


////// these are for a stream-based transport

// how many bytes are there ready to write, max is size*window
uint32_t chunks_len(chunks_t chunks);

// return the next block of data to be written to the stream transport, len is chunks_len()
uint8_t *chunks_write(chunks_t chunks);

// advance the write this far
chunks_t chunks_written(chunks_t chunks, uint32_t len);

// process incoming stream data into any packets, auto-generates acks that show up in _write()
chunks_t chunks_read(chunks_t chunks, uint8_t *block, uint32_t len);

#endif
