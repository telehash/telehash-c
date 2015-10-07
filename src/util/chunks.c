#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "telehash.h"

// one malloc per chunk, put storage after it
util_chunks_t util_chunk_new(util_chunks_t chunks, uint8_t size)
{
  util_chunk_t chunk;
  if(!(chunk = malloc(sizeof (struct util_chunk_struct) + size))) return LOG("OOM");
  memset(chunk,0,sizeof (struct util_chunk_struct) + size);
  chunk->size = size;
  chunk->data = ((void*)chunk)+(sizeof (struct util_chunk_struct));

  // add to reading list
  if(!chunks->cur)
  {
    chunks->reading = chunks->cur = chunk;
  }else{
    chunks->cur->next = chunk;
    chunks->cur = chunk;
  }
  chunks->readat = 0;

  return chunks;
}

util_chunk_t util_chunk_free(util_chunk_t chunk)
{
  if(!chunk) return NULL;
  util_chunk_t next = chunk->next;
  free(chunk);
  return util_chunk_free(next);
}

util_chunks_t util_chunks_new(uint8_t size)
{
  util_chunks_t chunks;
  if(!(chunks = malloc(sizeof (struct util_chunks_struct)))) return LOG("OOM");
  memset(chunks,0,sizeof (struct util_chunks_struct));
  chunks->blocked = 0;
  chunks->blocking = 1; // default

  if(!size)
  {
    chunks->cap = 255;
  }else if(size == 1){
    chunks->cap = 1; // minimum
  }else{
    chunks->cap = size-1;
  }

  return chunks;
}

util_chunks_t util_chunks_free(util_chunks_t chunks)
{
  if(!chunks) return NULL;
  if(chunks->writing) lob_free(chunks->writing);
  util_chunk_free(chunks->reading);
  free(chunks);
  return NULL;
}

uint32_t util_chunks_writing(util_chunks_t chunks)
{
  lob_t cur;
  uint32_t len = 0;
  if(!chunks) return 0;
  util_chunks_len(chunks); // flushes
  len = lob_len(chunks->writing) - chunks->writeat;
  for(cur=lob_next(chunks->writing);cur;cur = lob_next(cur)) len += lob_len(cur);
  return len;
}

// enable automatic cloaking
util_chunks_t util_chunks_cloak(util_chunks_t chunks)
{
  if(chunks) chunks->cloak = 1;
  return chunks;
}

util_chunks_t util_chunks_send(util_chunks_t chunks, lob_t out)
{
  if(!chunks || !out) return LOG("bad args");
  chunks->writing = lob_push(chunks->writing, out);
  // TODO cloaking, make lob internalize it
  return chunks;
}


// get any packets that have been reassembled from incoming chunks
lob_t util_chunks_receive(util_chunks_t chunks)
{
  util_chunk_t chunk, next;
  size_t len;
  lob_t ret;

  if(!chunks || !chunks->reading) return NULL;
  
  // find the first short chunk, extract packet
  for(len = 0,chunk = chunks->reading;chunk && chunk->size < chunks->cap;len += chunk->size,chunk = chunk->next);
  
  if(!chunk) return NULL;
  len += chunk->size; // meh
  
  // TODO make a lob_new that creates space to prevent double-copy here
  uint8_t *buf = malloc(len);
  if(len && !buf) return LOG("OOM");
  
  // eat chunks copying in
  for(len=0;chunks->reading != chunk;chunks->reading = next)
  {
    next = chunks->reading->next;
    memcpy(buf+len,chunks->reading->data,chunks->reading->size);
    len += chunks->reading->size;
    free(chunks->reading);
  }
  
  ret = (chunks->cloak)?lob_decloak(buf,len):lob_parse(buf,len);
  free(buf);
  return ret;
}

// internal to append read data
util_chunks_t _util_chunks_append(util_chunks_t chunks, uint8_t *block, size_t len)
{
  if(!chunks || !block || !len) return chunks;
  uint8_t quota = 0;
  
  if(chunks->cur) quota = chunks->cur->size - chunks->readat;
  
  // no space so add a new storage chunk
  if(!quota)
  {
    if(!util_chunk_new(chunks,*block)) return LOG("OOM");
    quota = *block;
    block++;
    len--;
    // a chunk was received, unblock and ack
    chunks->blocked = 0;
    chunks->ack = 1;
  }

  if(len < quota) quota = len;

  // copy in quota space
  memcpy(chunks->cur->data,block,quota);
  return _util_chunks_append(chunks,block+quota,len-quota);
}

// process an incoming individual chunk
util_chunks_t util_chunks_in(util_chunks_t chunks, uint8_t *chunk, uint8_t len)
{
  if(!chunks || !chunk || !len) return chunks;
  if(len < (*chunk + 1)) return LOG("invalid chunk len %d < %d+1",len,*chunk);
  return _util_chunks_append(chunks,chunk,len);
}

// how many bytes are there waiting
uint32_t util_chunks_len(util_chunks_t chunks)
{
  if(!chunks || chunks->blocked) return 0;

  // when no packet, only send an ack
  if(!chunks->writing) return (chunks->ack) ? 1 : 0;

  // what's the total left to write
  size_t avail = lob_len(chunks->writing) - chunks->writeat;

  // only deal w/ the next chunk
  if(avail > chunks->cap) avail = chunks->cap;
  if(!chunks->waiting) chunks->waiting = avail;

  // just writing the waiting size byte first
  if(!chunks->waitat) return 1;

  return chunks->waiting - (chunks->waitat-1);
}

// return the next block of data to be written to the stream transport
uint8_t *util_chunks_write(util_chunks_t chunks)
{
  // ensures consistency
  if(!util_chunks_len(chunks)) return NULL;
  
  // always write the chunk size byte first, is also the ack/flush
  if(!chunks->waitat) return &chunks->waiting;
  
  // into the raw data
  return lob_raw(chunks->writing)+chunks->writeat+(chunks->waitat-1);
}

// advance the write pointer this far
util_chunks_t util_chunks_written(util_chunks_t chunks, size_t len)
{
  if(!chunks || !len) return NULL;
  if(len > util_chunks_len(chunks)) return LOG("len too big %d > %d",len,util_chunks_len(chunks));
  chunks->waitat += len;
  chunks->ack = 0; // any write is an ack

  // if a chunk was done, advance to next chunk
  if(chunks->waitat > chunks->waiting)
  {
    // confirm we wrote the chunk data and size
    chunks->writeat += chunks->waiting;
    chunks->waiting = chunks->waitat = 0;

    // only block if it was a full chunk
    if(chunks->waiting == chunks->cap) chunks->blocked = chunks->blocking;

    // only advance packet after we wrote a flushing 0
    if(len == 1 && chunks->writeat == lob_len(chunks->writing))
    {
      lob_t next = lob_next(chunks->writing);
      lob_free(chunks->writing);
      chunks->writing = next;
      chunks->writeat = 0;
      // always block after a full packet
      chunks->blocked = chunks->blocking;
    }
  }
  
  return chunks;
}

// queues incoming stream based data
util_chunks_t util_chunks_read(util_chunks_t chunks, uint8_t *block, size_t len)
{
  if(!_util_chunks_append(chunks,block,len)) return NULL;
  if(!chunks->reading || !chunks->readat) return NULL; // paranoid
  return chunks;
}

