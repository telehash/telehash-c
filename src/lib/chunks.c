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
    chunks->writing[start] = size;
    start++;
    memcpy(chunks->writing+start,raw+at,size);
    at += size;
    start += size;
  }
  chunks->writing[start] = 0; // end of chunks, full packet
  
  return chunks;
}

// get any packets that have been reassembled from incoming chunks
lob_t chunks_receive(chunks_t chunks)
{
  uint32_t at, len;
  uint8_t *buf, *append;
  lob_t ret;

  if(!chunks || !chunks->reading) return NULL;
  // check for complete packet and get its length
  for(len = at = 0;at < chunks->readlen && chunks->reading[at]; at += chunks->reading[at]+1) len += chunks->reading[at];
  if(!len || at == chunks->readlen) return NULL;
  
  if(!(buf = malloc(len))) return LOG("OOM %d",len);
  // copy in the body of each chunk
  for(at = 0, append = buf; chunks->reading[at]; append += chunks->reading[at], at += chunks->reading[at]+1)
  {
    memcpy(append, chunks->reading+(at+1), chunks->reading[at]);
  }
  ret = lob_parse(buf,len);
  free(buf);
  
  // advance reading
  at++;
  chunks->readlen -= at;
  memmove(chunks->reading,chunks->reading+at,chunks->readlen);
  chunks->reading = util_reallocf(chunks->reading,chunks->readlen);

  return ret;
}

// how many bytes are there waiting to be sent total
uint32_t chunks_waiting(chunks_t chunks)
{
  if(!chunks || !chunks->writing) return 0;
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
  if(len[0] < chunks->size && chunks->writing[chunks->writeat+len[0]] == 0) len[0]++;
  ret = chunks->writing+chunks->writeat;
  chunks->writeat += len[0];
  return ret;
}

// free up some write buffer memory
chunks_t chunks_flush(chunks_t chunks, uint32_t len)
{
  if(!chunks || len > chunks->writelen) return NULL;
  if(len > chunks->writeat) chunks->writeat = len; // force-forward
  chunks->writelen -= len;
  chunks->writeat -= len;
  memmove(chunks->writing,chunks->writing+len,chunks->writelen);
  if(!(chunks->writing = util_reallocf(chunks->writing, chunks->writelen)))
  {
    chunks->writelen = chunks->writeat = 0;
    return LOG("OOM");
  }
  return chunks;
}

// >0 ack # of chunks that have been sent successfully, <0 drop oldest packet, 0 start-over/resend oldest packet
chunks_t chunks_ack(chunks_t chunks, int ack)
{
  uint32_t at, count;
  if(!chunks) return NULL;

  // reset
  if(ack == 0)
  {
    chunks->writeat = 0;
    chunks->acked = 0;
  }

  // nothing to do
  if(!chunks->writeat || !chunks->writelen) return chunks;

  // count how many chunks before next terminator
  for(at = count = 0;at < chunks->writelen && chunks->writing[at]; at += chunks->writing[at]+1) count++;

  // add one for terminator itself if it was alone in a chunk (on boundary)
  if(at % (chunks->size-1) == 0)
  {
    count++;
  }
  
  // include terminator
  at++;

  // clear current packet
  if(ack < 0)
  {
    chunks->acked = 0;
    return chunks_flush(chunks, at);
  }

  chunks->acked += ack;
  if(chunks->acked >= count)
  {
    chunks->acked -= count;
    return chunks_flush(chunks, at);
  }

  // not enough acks yet
  return chunks;
}

// process an incoming individual chunk
chunks_t chunks_chunk(chunks_t chunks, uint8_t *chunk, uint8_t len)
{
  if(!chunks || !chunk || !len) return chunks;
  if((*chunk + 1) != len) return LOG("invalid chunk len %d != %d+1",len,*chunk);
  chunks->readlen += len;
  if(!(chunks->reading = util_reallocf(chunks->reading, chunks->readlen))) return LOG("OOM"); 
  memcpy(chunks->reading+(chunks->readlen-len),chunk,len);
  return chunks;
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
  if(!chunks_len(chunks)) return NULL;
  return chunks->writing+chunks->writeat;
}

// advance the write pointer this far
chunks_t chunks_written(chunks_t chunks, uint32_t len)
{
  if(!chunks || (len+chunks->writeat) > chunks->writelen) return NULL;
  chunks->writeat += len;
  return chunks;
}

// process incoming stream data into any packets, auto-generates acks that show up in _write()
chunks_t chunks_read(chunks_t chunks, uint8_t *block, uint32_t len)
{
  uint32_t at, acks, acking, last;
  if(!chunks || !block || !len) return chunks;
  chunks->readlen += len;
  if(!(chunks->reading = util_reallocf(chunks->reading, chunks->readlen))) return LOG("OOM"); 
  memcpy(chunks->reading+(chunks->readlen-len),block,len);

  // process ack state since last read
  acks = acking = last = 0;
  for(at = chunks->readat;at < chunks->readlen; at += chunks->reading[at]+1)
  {
    if(chunks->reading[at] != 0 || last)
    {
      // sequential chunks must be acked
      acking++;
    }else{
      // lone zeros are incoming acks
      acks++;
    }
    last = chunks->reading[at];
  }
  chunks->readat = at;
  chunks_ack(chunks,acks);

  // auto-append acks back for any new chunks
  if(acking)
  {
    if(!(chunks->writing = util_reallocf(chunks->writing, chunks->writelen+acking))) return LOG("OOM");
    memset(chunks->writing+chunks->writelen,0,acking); // zeros are acks
    chunks->writelen += acking;
  }

  return chunks;
}
