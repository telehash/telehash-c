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
typedef struct ahead_s {
  union {
    struct {
      uint8_t packet : 1; // is this a packet frame (false is ignored, abstract can be used by transport)
      uint8_t start : 1; // start frame, abstract contains uint32_t len, data/state/hold special
      uint8_t end : 1; // last frame (if both, data follows self-contained)
      uint8_t data : 1; // replacement bit for data frame
      uint8_t state : 2; // default, sent, received, resend
      uint8_t hold : 2; // backpressure signalling
    };
    uint8_t bits;
  }
} ahead_s;

// abstract is header + checkhash
typedef struct abstract_s {
  ahead_s head;
  uint8_t hash[4];
} abstract_s, *abstract_t;

// sequence frames are array of abstracts
typedef struct sequence_s {
  uint8_t hash[4];
  abstract_s abs[]; // must remain uint32 aligned in this struct for murmur
} sequence_s, *sequence_t;

// overall manager state
struct util_frames_s {

  // checkhash seed to determine sequence frame direction
  uint32_t ours, theirs;

  // queue of packets ready for processing/delivery
  lob_t inbox;
  lob_t outbox;

  // sending in progress state
  sequence_t sending;
  uint8_t *out; // into outbox raw packet
  uint32_t len; // remaining in out

  // receiving in progress state
  sequence_t receiving;
  lob_t in; // full packet
  uint8_t *at; // current sequence into raw in packet
  
  // frame size and data frames per sequence ((frame_size-4)/5)
  uint16_t size;
  uint8_t per;

  // boolean flags
  uint8_t flush_in : 1;
  uint8_t flush_out : 1;
};

// for iterating
static uint8_t abs_next(sequence_t seq, uint8_t pos)
{
  // when supporting in-line data an abstract may cross multiple
  return pos+1;
}

#define abs_isframe(ab) (ab && ab->head.packet && !ab->head.start)

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

static sequence_t seq_free(sequence_t seq)
{
  if(seq) free(seq);
  return NULL;
}

// stages next packet to send from outbox 
static sequence_t seq_out(util_frames_t frames)
{
  if(!frames || !frames->outbox) return NULL;
  
  frames->out = lob_raw(frames->outbox);
  frames->len = lob_len(frames->outbox);
  sequence_t seq = frames->sending = seq_new(frames);

  // fill in sequence
  for(uint16_t i = 0, j = 0; i < frames->per; i++) {
    abstract_t ab = seq->abs[i];
    ab->head.packet = true;
    if(i == 0)
    {
      ab->head.start = true;
      memcpy(ab->head.hash, &(frames->len), 4);
      continue;
    }
    uint8_t *start = frames->out + (j * frames->size); // this frame starting point
    j++; // count of data frames done
    ab->head.data = (*start) & 0b10000000; // stash copy of first bit from data frame
    ab->head.state = AB_UNKNOWN;
    // last frame special handling
    if(j * frames->size >= frames->len)
    {
      ab->head.last = true;
      uint16_t rem = frames->len % frames->size;
      // remainder must hash full frame
      if(rem)
      {
        uint8_t *tmp = malloc(frames->size);
        memset(tmp,0,frames->size);
        memcpy(tmp, start, rem);
        murmur(frames->ours, tmp, frames->size, ab->hash);
        free(tmp);
        continue;
      }
    }
    murmur(frames->ours, start, frames->size, ab->hash);
  }
  murmur(frames->ours, (uint8_t*)(seq->abs), (sizeof(abstract_s) * frames->per, seq->hash);

  return frames;
}

util_frames_t util_frames_new(uint16_t size, uint32_t ours, uint32_t theirs)
{
  if(size < 24 || (size % 4) || (size - 4) % 5) return LOG_ERROR("invalid size: %u",size);
  if(ours == theirs) return LOG_ERROR("seeds must be different");

  util_frames_t frames;
  if(!(frames = malloc(sizeof (struct util_frames_struct)))) return LOG_WARN("OOM");
  memset(frames,0,sizeof (struct util_frames_struct));
  frames->ours = ours;
  frames->theirs = theirs;
  frames->size = size;
  frames->per = (size - 4) / 5;

  return frames;
}

util_frames_t util_frames_free(util_frames_t frames)
{
  if(!frames) return NULL;
  seq_free(receiving);
  seq_free(sending);
  lob_freeall(frames->inbox);
  lob_freeall(frames->outbox);
  lob_free(frames->in);
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
    frames->flush_out = 1;
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

  if(!frames->receiving) return len;

  // add in-progress data
  len += frames->at - lob_raw(frames->in);
  sequence_t s = frames->receiving;
  for(uint8_t i = 0; i < frames->per; i = abs_next(s,i))
    if(abs_isframe(&(s->abs[i])) && !s->abs[i].head.end && s->abs[i].head.state == AB_RECEIVED) len += frames->size;

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
  
  if(!frames->sending) return len;
  // subtract sent
  len -= frames->out - lob_raw(frames->outbox);
  sequence_t s = frames->sending;
  for(uint8_t i = 0; i < frames->per; i = abs_next(s, i))
    if(abs_isframe(&(s->abs[i])) && !s->abs[i].head.end && s->abs[i].head.state != AB_UNKNOWN) len -= frames->size;

  return len;
}

// is a frame pending to be sent immediately
util_frames_t util_frames_pending(util_frames_t frames)
{
  if(!frames) return LOG_WARN("bad args");
  if(frames->flush) return frames;
  if(!frames->sending) return LOG_CRAZY("empty");

  sequence_t s = frames->sending;
  for(uint8_t i = 0; i < frames->per; i = abs_next(s, i))
  {
    abstract_t ab = &(s->abs[i]);
    if(!abs_isframe(ab)) continue;
    if(!ab->head.hold) return LOG_CRAZY("held");
    if(ab->head.state == AB_UNKNOWN) return frames;
    if(ab->head.state == AB_RESEND) return frames;
  }

  return LOG_CRAZY("nothing");
}

// the next frame of data in/out, if data NULL bool is just ready check
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *raw)
{
  if(!frames) return LOG_WARN("bad args");
  if(!raw) return LOG_WARN("TODO old usage");
  
  // first, sequence or data frame
  if(raw[0] & 0b10000000)
  {
    // is a sequence frame, checkhash to decide who's
    uint8_t hash[4];

    murmur(frames->ours, raw+4, frames->size-4, hash);
    hash[0] |= 0b10000000;
    if(memcmp(raw,hash,4) == 0)
    {
      LOG_CRAZY("our sequence frame");
      return frames;
    }

    murmur(frames->theirs, raw + 4, frames->size - 4, hash);
    hash[0] |= 0b10000000;
    if(memcmp(raw, hash, 4) == 0) {
      LOG_CRAZY("their sequence frame");
      return frames;
    }

    LOG_DEBUG("RAW[%s]",util_hex(raw,frame->size,NULL));
    return LOG_INFO("checkhash failed on possible sequence frame");
  }

  // do we know how to receive data yet?
  if(!frames->receiving) return LOG_INFO("possible data frame but no receiving sequence");

  // is a data frame, check both possible hashes
  uint8_t zero[4], one[4];
  murmur(frames->theirs, raw, frames->size, zero);
  raw[0] |= 0b10000000;
  murmur(frames->theirs, raw, frames->size, one);

  sequence_t s = frames->receiving;
  uint8_t dpos = 0; // data frame position, 1-based index
  for(uint8_t i = 0; i < frames->per; i++)
  {
    if(!s->abs[i].head.frame) continue;
    dpos++;
    uint8_t *hash = (s->abs[i].head.data)?one:zero;
    // non-last data frames are simple compare
    if(!s->abs[i].head.last) {
      if(memcmp(s->abs[i].hash, hash) != 0) continue;
    }else{
      // last data frame is cumulative hash check
      for(uint16_t at = 0; at < frames->size; at += 4) {

      }
    }

    LOG_CRAZY("we've got ourselves a data frame");
    // set first bit back to zero if proper
    if(!s->abs[i].head.data) raw[0] &= 0b01111111;
    memcpy(s->data+(frames->size * (dpos-1)), raw, frames->size);
    s->abs[i].head.state = AB_RECEIVED;

    if(s->abs[i].head.last)
    // update status and done
    return frames;
  }

  return LOG_INFO("checkhash failed on possible data frame");

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

