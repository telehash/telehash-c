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
//  size_t tlen = (frames->inframes * frames->size);

  /*
  // TODO make a lob_new that creates space to prevent double-copy here
  uint8_t *buf = malloc(tlen);
  if(!buf) return LOG("OOM");
  
  // eat frames copying in
  util_frame_t prev;
  size_t at;
  for(at=len;frame;frame = prev)
  {
    prev = frame->prev;
    // backfill since we're inverted
    memcpy(buf+(at-frame->size),frame->data,frame->size);
    at -= frame->size;
    free(frame);
  }
  
  lob_t ret = lob_parse(buf,len);
  frames->err = ret ? 0 : 1;
  free(buf);
  return ret;
  */
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

//[60][4] normal data, if mmh is inverted it's a meta frame
// meta frame byte -5 (before mmh) is meta type
// <= 59 is packet done len
// == 60 is flush, first 4 is last good rx frame hash, next 4 last tx frame hash

// the next frame of data in/out, if data NULL bool is just ready check
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data)
{
  if(!frames) return LOG("bad args");
  if(!data) return (frames->inbox) ? frames : NULL;
  
  // check hash, if valid add frame and done
  // if valid after metamod, process meta
  //  if meta < PAYLOAD() process lob, set flush and done
  //  if meta == PAYLOAD() is an incoming flush
  //    if first hash is unknown, unrecoverable frame error
  //    if second hash is unknown, send flush
  return NULL;
}

util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data)
{
  if(!frames) return LOG("bad args");
  if(!data) return (frames->outbox || frames->flush) ? frames : NULL;
  
  // if flushing, send hashes and set meta flag
  // get next frame
  // if < PAYLOAD, set meta flag to size
  // else fill and hash

  return NULL;
}

/*
// internal to append read data
util_frames_t _util_frames_append(util_frames_t frames, uint8_t *block, size_t len)
{
  if(!frames || !block || !len) return frames;
  uint8_t quota = 0;
  
  // first, determine if block is a new frame or a remainder of a previous frame (quota > 0)
  if(frames->reading) quota = frames->reading->size - frames->readat;

//  LOG("frames append %d q %d",len,quota);
  
  // no space means we're at a frame start byte
  if(!quota)
  {
    if(!util_frame_new(frames,*block)) return LOG("OOM");
    // a frame was received, unblock
    frames->blocked = 0;
    // if it had data, flag to ack
    if(*block) frames->ack = 1;
    // start processing the data now that there's space
    return _util_frames_append(frames,block+1,len-1);
  }

  // only a partial data avail now
  if(len < quota) quota = len;

  // copy in quota space and recurse
  memcpy(frames->reading->data+frames->readat,block,quota);
  frames->readat += quota;
  return _util_frames_append(frames,block+quota,len-quota);
}

// how many bytes are there waiting
uint32_t util_frames_len(util_frames_t frames)
{
  if(!frames || frames->blocked) return 0;

  // when no packet, only send an ack
  if(!frames->writing) return (frames->ack) ? 1 : 0;

  // what's the total left to write
  size_t avail = lob_len(frames->writing) - frames->writeat;

  // only deal w/ the next frame
  if(avail > frames->cap) avail = frames->cap;
  if(!frames->waiting) frames->waiting = avail;

  // just writing the waiting size byte first
  if(!frames->waitat) return 1;

  return frames->waiting - (frames->waitat-1);
}

// return the next block of data to be written to the stream transport
uint8_t *util_frames_write(util_frames_t frames)
{
  // ensures consistency
  if(!util_frames_len(frames)) return NULL;
  
  // always write the frame size byte first, is also the ack/flush
  if(!frames->waitat) return &frames->waiting;
  
  // into the raw data
  return lob_raw(frames->writing)+frames->writeat+(frames->waitat-1);
}

// advance the write pointer this far
util_frames_t util_frames_written(util_frames_t frames, size_t len)
{
  if(!frames || !len) return frames;
  if(len > util_frames_len(frames)) return LOG("len too big %d > %d",len,util_frames_len(frames));
  frames->waitat += len;
  frames->ack = 0; // any write is an ack

//  LOG("frames written %d at %d ing %d",len,frames->waitat,frames->waiting);

  // if a frame was done, advance to next frame
  if(frames->waitat > frames->waiting)
  {
    // confirm we wrote the frame data and size
    frames->writeat += frames->waiting;
    frames->waiting = frames->waitat = 0;

    // only block if it was a full frame
    if(frames->waiting == frames->cap) frames->blocked = frames->blocking;

    // only advance packet after we wrote a flushing 0
    if(len == 1 && frames->writing && frames->writeat == lob_len(frames->writing))
    {
      lob_t old = lob_shift(frames->writing);
      frames->writing = old->next;
      old->next = NULL;
      lob_free(old);
      frames->writeat = 0;
      // always block after a full packet
      frames->blocked = frames->blocking;
    }
  }
  
  return frames;
}

// queues incoming stream based data
util_frames_t util_frames_read(util_frames_t frames, uint8_t *block, size_t len)
{
  if(!_util_frames_append(frames,block,len)) return NULL;
  if(!frames->reading) return NULL; // paranoid
  return frames;
}


////// these are for frame-based transport

// size of the next frame, -1 when none, max is frames size-1
int16_t util_frames_size(util_frames_t frames)
{
  if(!util_frames_len(frames)) return -1;
  return frames->waiting;
}

// return the next frame of data, use util_frames_next to advance
uint8_t *util_frames_frame(util_frames_t frames)
{
  if(!frames || !frames->waiting) return NULL;
  // into the raw data
  return lob_raw(frames->writing)+frames->writeat;
  
}

// process incoming frame
util_frames_t util_frames_data(util_frames_t frames, uint8_t *frame, int16_t size)
{
  if(!frames || size < 0) return NULL;
  if(!_util_frames_append(frames,(uint8_t*)&size,1)) return NULL;
  if(!_util_frames_append(frames,frame,size)) return NULL;
  return frames;
}

// peek into what the next frame size will be, to see terminator ones
int16_t util_frames_peek(util_frames_t frames)
{
  int16_t size = util_frames_size(frames);
  // TODO, peek into next frame
  if(size <= 0) return -1;
  return lob_len(frames->writing) - (frames->writeat+size);
}

// advance the write past the current frame
util_frames_t util_frames_next(util_frames_t frames)
{
  int16_t size = util_frames_size(frames);
  if(size < 0) return NULL;
  // header byte first, then full frame
  if(!util_frames_written(frames,1)) return NULL;
  if(!util_frames_written(frames,size)) return NULL;
  return frames;
}
*/
