#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "telehash.h"

/********** RECEIVING PROCESSING LOGIC ********************

        packet: a single variable length payload
         frame: configurable length
      sequence: one or more frames currently being transferred
          HHHH: murmur3 32bit checkhash
      abstract: 5 byte description of any frame
         BHHHH: frame abstract, leading byte of bitflags and four checkhash bytesH
sequence frame: used for exchanging state, array of abstracts and tail checkhash
               ┌─────────────────────────────────────────────────────────────────────┐
               │[BHHHH,BHHHH,BHHHH,...]                                          HHHH│
               └─────────────────────────────────────────────────────────────────────┘
    data frame: all payload data, checkhash must match a known abstract
  packet frame: contains end of packet, payload <= frame length


                                    ┌──────────────┐
                                    │receive frame │
                                    └──────────────┘

                                           Λ
                                    is sequence frame?
                                         ╱   ╲
                  ┌──────yes────────────▕     ▏───────────────────────────────────────no──────────────────────────────────┐
                  ▼                      ╲   ╱                                                                            ▼
                  Λ                       ╲ ╱                                                                             Λ
            who's sequence?                V                                                                       is data frame?
                ╱   ╲                                                                                                   ╱   ╲
        ┌──────▕     ▏────────────────────────theirs───────────────────┐                               ┌───no───────── ▕     ▏──────yes───┐
        │       ╲   ╱                                                  │                               │                ╲   ╱             │
      ours       ╲ ╱                                                   │                               │                 ╲ ╱              │
        │         V                                                    │                               │                  V               │
        ▼                                                              ▼                               ▼                                  ▼
┌──────────────┐                                               ┌──────────────┐                ┌──────────────┐                   ┌──────────────┐
│   compare    │                                               │   compare    │                │ all receive  │                   │  verify and  │
│ abstracts to │─mismatch────┐                   ┌───mismatch──│ abstracts to │                │    errors    │                   │  queue, set  │
│     ours     │             ▼                   ▼             │  last known  │                │+flush_theirs │                   │received state│
└──────────────┘     ┌──────────────┐    ┌──────────────┐      └──────────────┘                └──────────────┘                   └──────────────┘
        │            │              │    │ becomes new  │              │                                                                 │
        ▼            │ +flush_ours  │    │  receiving   │              ▼                                                                 │
┌──────────────┐     │              │    │   sequence   │      ┌──────────────┐                                                          │
│    update    │     └──────────────┘    └──────────────┘      │              │                                                          │    ┌──────────────┐
│  received &  │                                 │             │+flush_theirs │                                                          │    │  if packet   │
│resend states │                                 ▼             │              │                                                          ├───▶│  done/more,  │
└──────────────┘                         ┌──────────────┐      └──────────────┘                                                          │    │queue->packet │
        │                                │    clear     │              │                                                                 │    └──────────────┘
 frame received                          │ flush_theirs │       compare│states                                                           │    ┌──────────────┐
        │                                │              │              │                                                                 │    │if last frame │
        │    ┌──────────────┐            └──────────────┘              │    ┌──────────────┐                                             └───▶│ in sequence  │
        │    │is stop frame │                    │                     │    │ skip any we  │                                                  │+flush_theirs │
        ├───▶│ release sent │               check states               ├───▶│   have as    │                                                  └──────────────┘
        │    │    packet    │                    │   ┌──────────────┐  │    │   received   │
        │    └──────────────┘                    │   │ keep pre-set │  │    └──────────────┘
        │    ┌──────────────┐                    ├──▶│received state│  │    ┌──────────────┐
        │    │is last frame │                    │   │(lossy frames)│  │    │any sent state│
        └───▶│ in sequence, │                    │   └──────────────┘  └───▶│ sets resend  │
             │load next seq │                    │   ┌──────────────┐       │              │
             └──────────────┘                    │   │any sent state│       └──────────────┘
                                                 └──▶│ sets resend, │
                                                     │+flush_theirs │
                                                     └──────────────┘

orig notes:
  send:
    create frame_t list for packet
    fill with sequence frames for packet size / frame size
    set flush_send
  sending
    if flush_receive, send back their last sequence frame
    if flush_send, send our sequence frame 
    else send next frame in sequence w/ state resend
    else send next frame in sequence w/ state unknown
    else nothing
  receiving
    their sequence frame
      compare all abstracts, must match except state bits
        skip our received
        if sent, set resend
        flush_receive
      else replaces any existing sequence and clears flush_receive
        if state != unknown set resend and flush_receive
    data frame
      verify and add to queue
      update frame state bits, set received
    stop frame
      same as data
      process queue into packet
    last frame in sequence
      set flush_receive
      (leave sequence frame in place after complete in case of resend)
    any error
      set flush_receive
    our sequence frame
      compare all abstracts, must match except state bits (TODO: backpressure)
        their received and resend state overwrite ours
        if received a stop, release packet
        if received all, release sequence and start next if any
      else any mismatch set flush_send

packet done frame sizing
  progressive murmur 4byte checksum until match, size is actual bytes in last 4byte block

magic sizes that are x%4 and (x-4)%5
  24,44,64,84,104,124,144,164,184,204,224,244,264,284,...

high bit on first byte is 0=data, 1=sequence
  sequence abstract header contains replacement data bit 

our vs. their determined by sequence murmur seed

***************/

#define AB_UNKNOWN  0
#define AB_SENT     1
#define AB_RECEIVED 2
#define AB_RESEND   3

// single bit-packed header byte used in frame abstract
typedef struct ahead_s
{
  union {
    struct {
      uint8_t frame : 1; // is this a data frame (false is ignored, abstract can be used by transport)
      uint8_t data : 1; // replacement bit for data frame
      uint8_t state : 2; // default, sent, received, resend
      uint8_t hold : 1; // backpressure signalling
      uint8_t last : 1; // is this the last data frame for a packet
      uint8_t size : 2; // final byte length in last data frame
    };
    uint8_t bits;
  }
} ahead_s;

// abstract is header + checkhash
typedef struct abstract_s
{
  ahead_s head;
  uint8_t hash[4];
} abstract_s, *abstract_t;

// sequence frames are array of abstracts
typedef struct sequence_s {
  struct sequence_s *next;
  uint8_t *data; // the data in this sequence
  uint8_t hash[4];
  abstract_s abs[]; // must remain uint32 aligned in this struct for murmur
} sequence_s, *sequence_t;

// overall manager state
struct util_frames_s {

  lob_t inbox; // received packets waiting to be processed
  lob_t outbox; // current packet being sent out

  // active incoming/outgoing sequence lists
  sequence_t ins, outs;

  // current sequence in the in/out lists
  sequence_t receiving, sending;

  // checkhash seed to determine sequence frame ownership
  uint32_t ours, theirs;

  // frame size and data frames per sequence ((frame_size-4)/5)
  uint16_t size;
  uint8_t per;

  // boolean flags
  uint8_t flush : 1; // bool to signal a flush is needed
};

// create a single blank sequence
static sequence_t seq_new(util_frames_t frames)
{
  if(!frames) return NULL;
  sequence_t seq = NULL;
  uint16_t size = sizeof(sequence_s) + (sizeof(abstract_s) * frames->per);
  if(!(seq = malloc(size))) return LOG_WARN("OOM");
  memset(seq,0,size);
  return seq;
}

static sequence_t seq_free(sequence_t seq, bool data)
{
  if(!seq) return NULL;
  if(seq->next) seq_free(seq->next, data);
  if(data) free(seq->data); // their data is malloc'd
  free(seq);
  return NULL;
}

static sequence_t seq_out(util_frames_t frames)
{
  if(!frames || !frames->outbox) return NULL;
  
  seq_t out = NULL;
  seq_t cur = NULL;
  uint8_t *data = lob_raw(frames->outbox);
  // loop once per sequence
  for(uint16_t i=0;i<lob_len(frames->outbox);)
  {
    if(!out)
    {
      out = cur = seq_new(frames);
    }else{
      cur->next = seq_new(frames);
      cur = cur->next;
    }
    // loop once per frame in a sequence
    uint16_t j=0;
    for(;i<lob_len(frames->outbox) && j<frames->per;i+=frames->size,j++)
    {
      cur->abs[j].head.frame = true;
      cur->abs[j].head.data = data[i] & 0b10000000; // stash copy of first bit from data frame
      cur->abs[j].head.state = AB_UNKNOWN;
      // last frame
      if(i+frames->size >= lob_len(frames->outbox))
      {
        cur->abs[j].head.last = true;
        LOG_WARN("TODO cumulative hash and size");
        continue;
      }
      murmur(data + i, frames->size, cur->abs[j].hash);
    }
    murmur((uint8_t*)(cur->abs), (sizeof(abstract_s) * frames->per, cur->hash);
  }
}

util_frames_t util_frames_new(uint16_t size, uint32_t ours, uint32_t theirs)
{
  if(size < 24 || (size % 4) || (size - 4) % 5) return LOG_ERROR("invalid size: %u",size);
  if(ours == theirs) return LOG_ERROR("seeds must be different");

  util_frames_t frames;
  if(!(frames = malloc(sizeof (struct util_frames_struct)))) return LOG_WARN("OOM");
  memset(frames,0,sizeof (struct util_frames_struct));
  frames->size = size;
  frames->ours = ours;
  frames->theirs = theirs;

  return frames;
}

util_frames_t util_frames_free(util_frames_t frames)
{
  if(!frames) return NULL;
  seq_free(ins, true);
  seq_free(outs, false);
  lob_freeall(frames->inbox);
  lob_freeall(frames->outbox);
  free(frames);
  return NULL;
}

util_frames_t util_frames_send(util_frames_t frames, lob_t out)
{
  if(!frames) return LOG_WARN("bad args");
  
  if(out)
  {
    frames->outbox = lob_push(frames->outbox, out);
  }else{
    LOG_WARN("TODO remove any hold bits on frames->sending");
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
  
  // add in-progress data (estimate only)
  for(sequence_t s = frames->ins; s; s = s->next)
    for(uint8_t i = 0; i < frames->per; i++)
      if(s->abs[i].head.frame && s->abs[i].head.state == AB_RECEIVED) len += frames->size;

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
  for(sequence_t s = frames->outs; s; s = s->next)
    for(uint8_t i = 0; i < frames->per; i++)
      if(s->abs[i].head.frame && s->abs[i].head.state == AB_SENT) len -= frames->size;

  return len;
}

// is a frame pending to be sent immediately
bool util_frames_pending(util_frames_t frames)
{
  if(!frames) return LOG_WARN("bad args");
  if(frames->flush) return true;
  if(!frames->sending) return LOG_CRAZY("empty");

  sequence_t s = frames->sending;
  for(uint8_t i = 0; i < frames->per; i++)
    if(s->abs[i].head.frame) {
      if(!s->abs[i].head.hold) return LOG_CRAZY("held");
      if(s->abs[i].head.state != AB_SENT) return true;
    }

  return LOG_CRAZY("nothing");
}

// the next frame of data in/out, if data NULL bool is just ready check
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data, uint8_t *meta)
{
  if(!frames) return LOG_WARN("bad args");
  if(frames->err) return LOG_WARN("frame state error");
  if(!data) return util_frames_await(frames);
  
  // conveniences for code readability
  uint8_t size = PAYLOAD(frames);
  uint32_t hash1;
  memcpy(&(hash1),data+size,4);
  uint32_t hash2 = murmur4(data,size);
  uint32_t inlast = (frames->cache)?frames->cache->hash:frames->inbase;
  
//  LOG("frame sz %u hash rx %lu check %lu",size,hash1,hash2);
  
  // meta frames are self contained
  if(hash1 == hash2)
  {
//    LOG("meta frame %s",util_hex(data,size+4,NULL));

    // if requested, copy in metadata block
    if(meta) memcpy(meta,data+10,size-10);

    // verify sender's last rx'd hash
    uint32_t rxd;
    memcpy(&rxd,data,4);
    uint8_t *bin = lob_raw(frames->outbox);
    uint32_t len = lob_len(frames->outbox);
    uint32_t rxs = frames->outbase;
    uint8_t next = 0;
    do {
      // here next is always the frame to be re-sent, rxs is always the previous frame
      if(rxd == rxs)
      {
        frames->out = next;
        break;
      }

      // handle tail hash correctly like sender
      uint32_t at = next * size;
      rxs ^= murmur4((bin+at), ((at+size) > len) ? (len - at) : size);
      rxs += next;
      if(len < size) break;
    }while((++next) && (next*size) <= len);

    // it must have matched something above
    if(rxd != rxs)
    {
      LOG_WARN("invalid received frame hash %lu check %lu",rxd,rxs);
      frames->err = 1;
      return NULL;
    }
    
    // advance full packet once confirmed
    if((frames->out * size) > len)
    {
      frames->out = 0;
      frames->outbase = rxd;
      lob_t done = lob_shift(frames->outbox);
      frames->outbox = done->next;
      done->next = NULL;
      lob_free(done);
    }

    // sender's last tx'd hash mismatch causes flush
    memcpy(&rxd,data+4,4);
    if(rxd != inlast)
    {
      frames->flush = 1;
      LOG_DEBUG("flushing mismatch, hash %lu last %lu",rxd,inlast);
    }
    
    // update more flag (TODO move incoming byte to bitfield)
    frames->more = (data[8])?1:0;

    return frames;
  }
  
  // dedup, ignore if identical to any received one
  if(hash1 == frames->inbase) return frames;
  util_frame_t cache = frames->cache;
  for(;cache;cache = cache->prev) if(cache->hash == hash1) return frames;

  // full data frames must match combined w/ previous
  hash2 ^= inlast;
  hash2 += frames->in;
  if(hash1 == hash2)
  {
    if(!util_frame_new(frames)) return LOG_WARN("OOM");
    // append, update inlast, continue
    memcpy(frames->cache->data,data,size);
    frames->cache->hash = hash1;
    frames->flush = 0;
//    LOG("got data frame %lu",hash1);
    return frames;
  }
  
  // check if it's a tail data frame
  uint8_t tail = data[size-1];
  if(tail >= size)
  {
    frames->flush = 1;
    return LOG_DEBUG("invalid frame %u tail %u >= %u hash %lu/%lu base %lu last %lu",frames->in,tail,size,hash1,hash2,frames->inbase,inlast);
  }
  
  // hash must match
  hash2 = murmur4(data,tail);
  hash2 ^= inlast;
  hash2 += frames->in;
  if(hash1 != hash2)
  {
    frames->flush = 1;
    return LOG_DEBUG("invalid frame %u tail %u hash %lu != %lu base %lu last %lu",frames->in,tail,hash1,hash2,frames->inbase,inlast);
  }
  
  // process full packet w/ tail, update inlast, set flush
//  LOG("got frame tail of %u",tail);
  frames->flush = 1;
  frames->inbase = hash1;
  frames->more = 0; // clear any more flag also

  size_t tlen = (frames->in * size) + tail;

  // TODO make a lob_new that creates space to prevent double-copy here
  uint8_t *buf = malloc(tlen);
  if(!buf) return LOG_WARN("OOM");
  
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
  if(!packet) LOG_WARN("packet parsing failed: %s",util_hex(buf,tlen,NULL));
  free(buf);
  frames->inbox = lob_push(frames->inbox,packet);
  return frames;
}

util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data, uint8_t *meta)
{
  if(!frames) return LOG_WARN("bad args");
  if(frames->err) return LOG_WARN("frame state error");
  if(!data) return util_frames_waiting(frames); // just a ready check
  uint8_t size = PAYLOAD(frames);
  uint8_t *out = lob_raw(frames->outbox);
  uint32_t len = lob_len(frames->outbox); 
  
  // clear/init
  uint32_t hash = frames->outbase;
  
  // first get the last sent hash
  if(len)
  {
    // safely only hash the packet size correctly
    uint32_t at, i;
    for(i = at = 0;at < len && i < frames->out;i++,at += size)
    {
      hash ^= murmur4((out+at), ((at - len) < size) ? (at - len) : size);
      hash += i;
    }
  }

  // if flushing, or nothing to send, just send meta frame w/ hashes
  if(frames->flush || !len || (frames->out * size) > len)
  {
    frames->flush = 1; // so _sent() does us proper
    memset(data,0,size+4);
    uint32_t inlast = (frames->cache)?frames->cache->hash:frames->inbase;
    memcpy(data,&(inlast),4);
    memcpy(data+4,&(hash),4);
    if(len && (frames->out * size) <= len) data[8] = 1; // flag we have more to send
    if(meta) memcpy(data+10,meta,size-10);
    murmur(data,size,data+size);
    LOG_CRAZY("sending meta frame inlast %lu cur %lu",inlast,hash);
    return frames;
  }
  
  // send next frame
  memset(data,0,size+4);
  uint32_t at = frames->out * size;
  if((at + size) > len)
  {
    size = len - at;
    data[PAYLOAD(frames)-1] = size;
  }
  // TODO there's extra space in tail frames that could be used for meta
  memcpy(data,out+at,size);
  hash ^= murmur4(data,size);
  hash += frames->out;
  memcpy(data+PAYLOAD(frames),&(hash),4);
  LOG_CRAZY("sending data frame %u %lu",frames->out,hash);

  return frames;
}

// out state changes, returns if more to send
util_frames_t util_frames_sent(util_frames_t frames)
{
  if(!frames) return LOG_WARN("bad args");
  if(frames->err) return LOG_WARN("frame state error");
  uint8_t size = PAYLOAD(frames);
  uint32_t len = lob_len(frames->outbox); 
  uint32_t at = frames->out * size;

  // we sent a meta-frame, clear flush and done
  if(frames->flush || !len || at > len)
  {
    frames->flush = 0;
    return NULL;
  }

  // else advance payload
  if((at + size) > len) size = len - at;
  frames->outbox->id = at + size; // track exact sent bytes
  frames->out++; // advance sent frames counter

  // if no more, signal done
  if((frames->out * size) > len) return NULL;
  
  // more to go
  return frames;
}

// busy check, in or out
util_frames_t util_frames_busy(util_frames_t frames)
{
  if(!frames) return NULL;
  if(util_frames_waiting(frames)) return frames;
  return util_frames_await(frames);
}

