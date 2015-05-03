#ifndef util_chunks_h
#define util_chunks_h

#include <stdint.h>
#include "lob.h"

typedef struct util_chunks_struct *util_chunks_t;

// size of each chunk, 0 == MAX (256)
util_chunks_t util_chunks_new(uint8_t size);

util_chunks_t util_chunks_free(util_chunks_t chunks);

// enable automatic cloaking
util_chunks_t util_chunks_cloak(util_chunks_t chunks);

// turn this packet into chunks and append
util_chunks_t util_chunks_send(util_chunks_t chunks, lob_t out);

// get any packets that have been reassembled from incoming chunks
lob_t util_chunks_receive(util_chunks_t chunks);


////// these are for sending on a transport that is message/frame based, always read first before writing to prevent deadlocks

// get the next chunk, put its length in len
uint8_t *util_chunks_out(util_chunks_t chunks, uint8_t *len);

// manually advances to the next outgoing chunk
util_chunks_t util_chunks_next(util_chunks_t chunks);

// process an incoming individual chunk
util_chunks_t util_chunks_in(util_chunks_t chunks, uint8_t *chunk, uint8_t len);


////// these are for a stream-based transport

// how many bytes are there ready to write (multiple chunks)
uint32_t util_chunks_len(util_chunks_t chunks);

// return the next block of data to be written to the stream transport, max len is util_chunks_len()
uint8_t *util_chunks_write(util_chunks_t chunks);

// advance the write this far, don't mix with util_chunks_out() usage
util_chunks_t util_chunks_written(util_chunks_t chunks, size_t len);

// queues incoming stream based data
util_chunks_t util_chunks_read(util_chunks_t chunks, uint8_t *block, size_t len);

// sends an ack if neccessary, after any more chunks have been received and none waiting to send
util_chunks_t util_chunks_ack(util_chunks_t chunks);

#endif
