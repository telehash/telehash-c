#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "telehash.h"

// max payload size per frame
#define PAYLOAD(f) (f->size - 4)

// one malloc per frame, put storage after it
util_frames_t util_frame_new(util_frames_t frames)
{
  util_frame_t frame;
  size_t size = sizeof (struct util_frame_struct);
  size += PAYLOAD(frames);
  if(!(frame = malloc(size))) return LOG("OOM");
  memset(frame,0,size);
  
  // add to inbox
  frame->prev = frames->cache;
  frames->cache = frame;
  frames->in++;

  return frames;
}

util_frame_t util_frame_free(util_frame_t frame)
{
  if(!frame) return NULL;
  util_frame_t prev = frame->prev;
  free(frame);
  return util_frame_free(prev);
}

util_frames_t util_frames_new(uint8_t size)
{
  if(size < 16 || size > 128) return LOG("invalid size: %u",size);

  util_frames_t frames;
  if(!(frames = malloc(sizeof (struct util_frames_struct)))) return LOG("OOM");
  memset(frames,0,sizeof (struct util_frames_struct));
  frames->size = size;

  // default init hash state
  frames->inhash = frames->outhash = 42;

  return frames;
}

util_frames_t util_frames_free(util_frames_t frames)
{
  if(!frames) return NULL;
  lob_freeall(frames->inbox);
  lob_freeall(frames->outbox);
  util_frame_free(frames->cache);
  free(frames);
  return NULL;
}

util_frames_t util_frames_send(util_frames_t frames, lob_t out)
{
  if(!frames) return LOG("bad args");
  if(frames->err) return LOG("stream broken");
  
  if(out) frames->outbox = lob_push(frames->outbox, out);
  else frames->flush = 1;

  return frames;
}

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames)
{
  if(!frames || !frames->inbox) return NULL;
  lob_t pkt = lob_shift(frames->inbox);
  frames->inbox = pkt->next;
  pkt->next = NULL;
  return pkt;
}

void frames_lob(util_frames_t frames, uint8_t *tail, uint8_t len)
{  
}

// total bytes in the inbox/outbox
size_t util_frames_inlen(util_frames_t frames)
{
  if(!frames) return 0;

  size_t len = 0;
  lob_t cur = frames->inbox;
  do {
    len += lob_len(cur);
    cur = lob_next(cur);
  }while(cur);
  
  // add cached frames
  len += (frames->in * PAYLOAD(frames));
  
  return len;
}

size_t util_frames_outlen(util_frames_t frames)
{
  if(!frames) return 0;
  size_t len = 0;
  lob_t cur = frames->outbox;
  do {
    len += lob_len(cur);
    cur = lob_next(cur);
  }while(cur);
  
  // subtract sent
  len -= (frames->out * PAYLOAD(frames));

  return len;
}

// the next frame of data in/out, if data NULL bool is just ready check
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data)
{
  if(!frames) return LOG("bad args");
  if(frames->err) return LOG("stream broken");
  if(!data) return (frames->inbox) ? frames : NULL;
  uint8_t size = PAYLOAD(frames); // easier to read
  
  // bundled hash
  uint32_t fhash;
  memcpy(&fhash,data+size,4);

  // check hash
  uint32_t hash = murmur4((uint32_t*)data,size);
  
  // meta frames are self contained
  if(fhash == hash)
  {
    LOG("meta frame %s",util_hex(data,size,NULL));
    // verify sender's last rx'd hash
    uint32_t rxd;
    memcpy(&rxd,data,4);
    uint8_t *bin = lob_raw(frames->outbox);
    uint32_t rxs = frames->outhash;
    uint8_t i;
    for(i = 0;i < frames->out;i++)
    {
      // verify/reset to last rx'd frame
      if(rxd == rxs)
      {
        frames->out = i;
        break;
      }
      rxs ^= murmur4((uint32_t*)(bin+(size*i)),size);
    }
    if(rxd != rxs)
    {
      LOG("invalid received frame hash, broken stream");
      frames->err = 1;
      return NULL;
    }

    // sender's last tx'd hash changes flush state
    uint32_t txd;
    memcpy(&txd,data+4,4);
    frames->flush = (txd == frames->inhash) ? 0 : 1;
    
    return frames;
  }
  
  // full data frames must match combined w/ previous
  if(fhash == (hash ^ frames->inhash))
  {
    if(!util_frame_new(frames)) return LOG("OOM");
    // append, update inhash, continue
    memcpy(frames->cache->data,data,size);
    frames->flush = 0;
    frames->inhash = fhash;
    return frames;
  }
  
  // check if it's a tail data frame
  uint8_t tail = data[PAYLOAD(frames)-1];
  if(tail >= PAYLOAD(frames))
  {
    frames->flush = 1;
    return LOG("invalid frame data length: %u",tail);
  }
  
  // hash must match
  hash = murmur4((uint32_t*)data,tail);
  if(fhash != (hash ^ frames->inhash))
  {
    frames->flush = 1;
    return LOG("invalid frame data hash");
  }
  
  // process full packet w/ tail, update inhash, set flush
  frames->flush = 1;
  frames->inhash = fhash;
  
  size_t tlen = (frames->in * size) + tail;

  // TODO make a lob_new that creates space to prevent double-copy here
  uint8_t *buf = malloc(tlen);
  if(!buf) return LOG("OOM");
  
  // copy in tail
  memcpy(buf+(frames->in * size), data, tail);
  
  // eat cached frames copying in reverse
  util_frame_t frame = frames->cache;
  uint8_t i = frames->in;
  while(i && frame)
  {
    i--;
    memcpy(buf+(i*size),frame->data,size);
    frame = frame->prev;
  }
  frames->cache = util_frame_free(frames->cache);
  
  lob_t packet = lob_parse(buf,tlen);
  if(!packet) LOG("packet parsing failed: %s",util_hex(buf,tlen,NULL));
  free(buf);
  frames->inbox = lob_push(frames->inbox,packet);
  return frames;
}

util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data)
{
  if(!frames) return LOG("bad args");
  if(frames->err) return LOG("stream broken");
  if(!data) return (frames->outbox || frames->flush) ? frames : NULL;

  // clear
  uint8_t size = PAYLOAD(frames);
  memset(data,0,size);
  
  // first get the last sent hash
  uint32_t hash = frames->outhash;
  uint8_t *bin = lob_raw(frames->outbox);
  uint8_t i;
  for(i = 0;i < frames->out;i++) hash ^= murmur4((uint32_t*)(bin+(size*i)),size);

  // if flushing, just send hashes
  if(frames->flush)
  {
    memcpy(data,&frames->inhash,4);
    memcpy(data+4,&hash,4);
    murmur(data,size,data+size);
    return frames;
  }
  
  // nothing to send
  if(!bin) return NULL;

  // get next frame
  uint32_t at = frames->out * size;
  uint32_t diff = lob_len(frames->outbox) - at;
  // if < PAYLOAD, set meta flag
  if(diff < size)
  {
    memcpy(data,bin,diff);
    data[size-1] = diff;
    hash ^= murmur4((uint32_t*)data,diff);
  }else{
    memcpy(data,bin,size);
    hash ^= murmur4((uint32_t*)data,size);
  }
  memcpy(data+size,&hash,4);

  return frames;
}

