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
  frames->inlast = frames->outbase = 42;

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
  
  if(out)
  {
    out->id = 0; // used to track sent bytes
    frames->outbox = lob_push(frames->outbox, out);
  }else{
    frames->flush = 1;
  }

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
  if(frames->outbox) len -= frames->outbox->id;

  return len;
}

// the next frame of data in/out, if data NULL bool is just ready check
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data)
{
  if(!frames) return LOG("bad args");
  if(frames->err) return LOG("stream broken");
  if(!data) return (frames->inbox) ? frames : NULL;
  
  // conveniences for code readability
  uint8_t size = PAYLOAD(frames);
  uint8_t size32 = size/4;
  uint32_t *data32 = (uint32_t*)data;
  uint32_t hash1 = data32[size32];
  uint32_t hash2 = murmur4(data,size);
  
  LOG("frame hash rx %lu check %lu",hash1,hash2);
  LOG("frame last %lu",frames->inlast);
  
  // meta frames are self contained
  if(hash1 == hash2)
  {
    LOG("meta frame %s",util_hex(data,size+4,NULL));
    // verify sender's last rx'd hash
    uint32_t rxd = data32[0];
    uint8_t *bin = lob_raw(frames->outbox);
    uint32_t len = lob_len(frames->outbox);
    uint32_t rxs = frames->outbase;
    uint8_t i;
    for(i = 0;i < frames->out;i++)
    {
      // verify/reset to last rx'd frame
      if(rxd == rxs)
      {
        frames->out = i;
        break;
      }

      // handle tail hash correctly like sender
      uint32_t at = i * size;
      rxs ^= murmur4((bin+at), ((at+size) > len) ? (len - at) : size);
      rxs += i;
    }
    if(rxd != rxs)
    {
      LOG("invalid received frame hash %lu check %lu",rxd,rxs);
      frames->err = 1;
      return NULL;
    }
    
    // advance full packet once confirmed
    if((frames->out * size) > len)
    {
      LOG("packet confirmed");
      frames->out = 0;
      frames->outbase = rxd;
      lob_t done = lob_shift(frames->outbox);
      frames->outbox = done->next;
      done->next = NULL;
      lob_free(done);
    }

    // sender's last tx'd hash changes flush state
    if(data32[1] == frames->inlast)
    {
      frames->flush = 0;
      LOG("data frame confirmed");
    }else{
      frames->flush = 1;
      LOG("flushing mismatch, hash %lu last %lu",data32[1],frames->inlast);
    }
    
    return frames;
  }
  
  // full data frames must match combined w/ previous
  hash2 ^= frames->inlast;
  hash2 += frames->in;
  if(hash1 == hash2)
  {
    if(!util_frame_new(frames)) return LOG("OOM");
    // append, update inlast, continue
    memcpy(frames->cache->data,data,size);
    frames->flush = 0;
    frames->inlast = hash1;
    LOG("got data frame %lu",hash1);
    return frames;
  }
  
  // check if it's a tail data frame
  uint8_t tail = data[size-1];
  if(tail >= size)
  {
    frames->flush = 1;
    return LOG("invalid frame data length: %u %s",tail,util_hex(data+(size-4),8,NULL));
  }
  
  // hash must match
  hash2 = murmur4(data,tail);
  hash2 ^= frames->inlast;
  hash2 += frames->in;
  if(hash1 != hash2)
  {
    frames->flush = 1;
    return LOG("invalid frame tail (%u) hash %lu tail %u ",tail,hash1,hash2);
  }
  
  // process full packet w/ tail, update inlast, set flush
  LOG("got frame tail of %u",tail);
  frames->flush = 1;
  frames->inlast = hash1;
  
  size_t tlen = (frames->in * size) + tail;

  // TODO make a lob_new that creates space to prevent double-copy here
  uint8_t *buf = malloc(tlen);
  if(!buf) return LOG("OOM");
  
  // copy in tail
  memcpy(buf+(frames->in * size), data, tail);
  
  // eat cached frames copying in reverse
  util_frame_t frame = frames->cache;
  while(frames->in && frame)
  {
    frames->in--;
    memcpy(buf+(frames->in*size),frame->data,size);
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
  uint8_t size = PAYLOAD(frames);
  uint8_t size32 = size/4;
  uint32_t *data32 = (uint32_t*)data;
  uint8_t *out = lob_raw(frames->outbox);
  uint32_t len = lob_len(frames->outbox); 
  
  // is just a check to see if there's something to send
  if(!data)
  {
    if(frames->flush) return frames;
    if(len && (frames->out * size) <= len) return frames;
    return NULL;
  }

  // clear/init
  memset(data,0,size);
  uint32_t hash = frames->outbase;
  
  // first get the last sent hash
  if(out && len)
  {
    // safely only hash the packet size correctly
    uint32_t at, i;
    for(i = at = 0;at < len && i < frames->out;i++,at += size)
    {
      hash ^= murmur4((out+at), ((at - len) < size) ? (at - len) : size);
      hash += i;
    }
  }

  // if flushing, just send hashes
  if(frames->flush)
  {
    data32[0] = frames->inlast;
    data32[1] = hash;
    data32[size32] = murmur4(data,size);
    LOG("sending meta frame %s",util_hex(data,size+4,NULL));
    LOG("check hash %u",data32[size32]);
    LOG("inlast %u hash %u",frames->inlast,hash);
    frames->flush = 0;
    return frames;
  }
  
  // nothing to send
  if(!out || !len || (frames->out * size) > len) return LOG("no data to send %u/%lu",frames->out,len);

  // send next frame
  uint32_t at = frames->out * size;
  if((at + size) > len)
  {
    size = len - at;
    data[PAYLOAD(frames)-1] = size;
  }
  memcpy(data,out+at,size);
  hash ^= murmur4(data,size);
  hash += frames->out;
  data32[size32] = hash;
  LOG("sending data frame %u %lu",frames->out,hash);
  frames->out++; // sent frames
  frames->outbox->id = at + size; // track exact sent bytes too

  return frames;
}

// is there an expectation of an incoming frame
util_frames_t util_frames_await(util_frames_t frames)
{
  if(!frames) return NULL;
  // need more to complete inbox
  if(frames->cache) return frames;
  // outbox is complete, awaiting flush
  if((frames->out * PAYLOAD(frames)) > lob_len(frames->outbox)) return frames;
  return NULL;
}

