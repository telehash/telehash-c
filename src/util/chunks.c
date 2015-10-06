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

  if(!size)
  {
    chunks->space = 255;
  }else if(size == 1){
    chunks->space = 1; // minimum
  }else{
    chunks->space = size-1;
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
  for(len = 0,chunk = chunks->reading;chunk && chunk->size < chunks->space;len += chunk->size,chunk = chunk->next);
  
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

// get the next chunk, put its length in len
uint8_t *util_chunks_out(util_chunks_t chunks, uint8_t *len)
{
  if(!chunks || !len) return NULL;
  len[0] = 0;

  // blocked
  if(chunks->blocked) return NULL;

  if(!util_chunks_len(chunks)) return NULL;
  len[0] = chunks->waiting;
  return lob_raw(chunks->writing)+chunks->writeat;
}

// clears out block
util_chunks_t util_chunks_next(util_chunks_t chunks)
{
  if(chunks) chunks->blocked = 0;
  return chunks;
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
  if(!chunks || !chunks->writing) return 0;
  size_t avail = lob_len(chunks->writing) - chunks->writeat;

  // only deal w/ the current chunk
  if(avail > chunks->space) avail = chunks->space;
  if(!chunks->waiting) chunks->waiting = avail;
  if(!chunks->waitat) return 1; // just the chunk size byte
  return chunks->waiting - chunks->waitat;
}

// return the next block of data to be written to the stream transport
uint8_t *util_chunks_write(util_chunks_t chunks)
{
  if(!util_chunks_len(chunks)) return NULL;
  
  // always write the chunk size byte first
  if(!chunks->waitat) return &chunks->waiting;
  return lob_raw(chunks->writing)+chunks->writeat+chunks->waitat;
}

// advance the write pointer this far
util_chunks_t util_chunks_written(util_chunks_t chunks, size_t len)
{
  if(!chunks) return NULL;
  if(len > util_chunks_len(chunks)) return LOG("len too big %d > %d",len,util_chunks_len(chunks));
  chunks->waitat += len;
  if(chunks->waitat > chunks->waiting)
  {
    chunks->writeat += chunks->waiting;
    chunks->waiting = chunks->waitat = 0;
    if(chunks->writeat >= lob_len(chunks->writing))
    {
      lob_t next = lob_next(chunks->writing);
      lob_free(chunks->writing);
      chunks->writing = next;
      chunks->writeat = 0;
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


// sends an ack if neccessary, after any more chunks have been received and none waiting to send
util_chunks_t util_chunks_ack(util_chunks_t chunks)
{
  if(!chunks) return NULL;
  
  // skip ack if anything waiting
  if(chunks->writing) return chunks;
  
  // flag to send an ack
  chunks->ack = 1;

  // implicitly unblock after any new chunks
//  util_chunks_next(chunks);

  // don't ack if the last received was an ack
//  if(zeros > 1 && (count - chunks->acked) == 1) return NULL;

  return chunks;
  
}
