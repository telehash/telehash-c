#include "chunks.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "util.h"
#include "platform.h"

#define CEIL(a, b) (((a) / (b)) + (((a) % (b)) > 0 ? 1 : 0))

struct chunks_struct
{
  uint8_t size;
  uint32_t window, acked;
  uint8_t *writing;
  uint32_t writelen, writeat;
  uint8_t *reading;
  uint32_t readlen, readat;
};

chunks_t chunks_new(uint8_t size, uint32_t window)
{
  chunks_t chunks;
  if(size < 2) return LOG("size must be >1");
  if(!window) return LOG("window must be >0");
  if(!(chunks = malloc(sizeof (struct chunks_struct)))) return LOG("OOM");
  memset(chunks,0,sizeof (struct chunks_struct));

  chunks->size = size;
  chunks->window = window;
  return chunks;
}

chunks_t chunks_free(chunks_t chunks)
{
  if(!chunks) return NULL;
  if(chunks->writing) free(chunks->writing);
  if(chunks->reading) free(chunks->reading);
  free(chunks);
  return NULL;
}


// turn this packet into chunks
chunks_t chunks_send(chunks_t chunks, lob_t out)
{
  uint32_t start, len, at;
  uint8_t *raw, size, max;
  if(!chunks || !(len = lob_len(out))) return chunks;
  max = chunks->size-1;
  start = chunks->writelen;
  chunks->writelen += len;
  chunks->writelen += CEIL(len,max); // space for per-chunk start byte and optional packet term
  chunks->writelen++; // space for terminating 0
  if(!(chunks->writing = util_reallocf(chunks->writing, chunks->writelen)))
  {
    chunks->writelen = chunks->writeat = 0;
    return LOG("OOM");
  }
  
  raw = lob_raw(out);
  for(at = 0; at < len;)
  {
    size = ((len-at) < max) ? (len-at) : max;
    chunks->writing[start+at] = size;
    at++;
    memcpy(chunks->writing+(start+at),raw+at,size);
    at += size;
  }
  chunks->writing[start+at] = 0; // end of chunks, full packet
  
  return chunks;
}

// get any packets that have been reassembled from incoming chunks
lob_t chunks_receive(chunks_t chunks)
{
  return NULL;
}

// how many bytes are there waiting to be sent total
uint32_t chunks_waiting(chunks_t chunks)
{
  if(!chunks) return 0;
  return chunks->writelen - chunks->writeat;
}

// get the next chunk, put its length in len, advances
uint8_t *chunks_next(chunks_t chunks, uint8_t *len)
{
  uint8_t *ret;
  if(!chunks || !len) return NULL;
  if(chunks->writeat == chunks->writelen)
  {
    len[0] = 0;
    return NULL;
  }
  len[0] = chunks->writing[chunks->writeat] + 1;
  // include the packet terminator in this chunk if there's space
  if(len[0] < 255 && chunks->writing[chunks->writeat+len[0]] == 0) len[0]++;
  ret = chunks->writing+chunks->writeat;
  chunks->writeat += len[0];
  return ret;
}

// >0 ack that a chunk was sent, <0 drop all the packets partially written, 0 start-over/resend current window
chunks_t chunks_ack(chunks_t chunks, int ack)
{
  if(!chunks) return NULL;
  return NULL;
}

// process an incoming individual chunk, also parses as a raw lob as a fallback
chunks_t chunks_chunk(chunks_t chunks, uint8_t *chunk, uint8_t len)
{
  return NULL;
}

// how many bytes are there READY to be sent
uint32_t chunks_len(chunks_t chunks)
{
  uint32_t max, len;
  if(!chunks) return 0;
  max = chunks->size * chunks->window;
  len = chunks_waiting(chunks);
  return (max < len) ? max : len;
}

// return the next block of data to be written to the stream transport
uint8_t *chunks_write(chunks_t chunks)
{
  return NULL;
}

// advance the write this far
chunks_t chunks_written(chunks_t chunks, uint32_t len)
{
  return NULL;
}

// process incoming stream data into any packets, auto-generates acks that show up in _write()
chunks_t chunks_read(chunks_t chunks, uint8_t *block, uint32_t len)
{
  return NULL;
}
