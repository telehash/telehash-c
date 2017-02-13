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

// state bits
#define AB_UNKNOWN  0
#define AB_SENT     1
#define AB_RECEIVED 2
#define AB_RESEND   3

// hold bits
#define AB_PROCEED  0
#define AB_HOLD     1
#define AB_PAUSE    2
#define AB_RESTART  3

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
  lob_t pkt;
  uint8_t *data;
  uint32_t len;
  uint16_t count; // received data frames since seq started, good or bad
  struct state {
    uint8_t abid; // references an abs[abid] for below states
    uint8_t do_flush : 1;
    uint8_t is_held : 1;
    uint8_t is_paused : 1;
    uint8_t out_sending : 1;
  };
  uint8_t hash[4];
  abstract_s abs[]; // must remain uint32 aligned in this struct for murmur
} sequence_s, *sequence_t;

// overall manager state
struct util_frames_s {

  // checkhash seed to determine sequence frame direction
  uint32_t ours, theirs;

  // a single queued packet ready for processing/delivery
  lob_t inbox;
  lob_t outbox;

  // in-progress states
  sequence_t sending, receiving;
  
  // frame size and frames per sequence ((frame_size-4)/5)
  uint16_t size;
  uint8_t per;

  // boolean flags
  uint8_t do_reset : 1;
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
static util_frames_t seq_out(util_frames_t frames)
{
  if(!frames) return NULL;
  sequence_t seq = frames->sending;

  // start w/ new packet if none staged
  if(!seq)
  {
    if(!frames->outbox) return LOG_DEBUG("no packet to sequence");
    seq = frames->sending = seq_new(frames);
    seq->pkt = frames->outbox;
    frames->outbox = NULL;
    seq->data = lob_raw(seq->pkt);
    seq->len = lob_len(seq->pkt);
  }


  // fill in this sequence's abstracts
  for(uint8_t i = 0, j = 0; i < frames->per; i++) {
    abstract_t ab = seq->abs[i];
    ab->head.packet = true;

    // very first abstract is start and contains len
    if(i == 0 && seq->len == lob_len(seq->pkt))
    {
      ab->head.start = true;
      memcpy(ab->head.hash, &(seq->len), 4);
      continue;
    }

    uint8_t *start = seq->data + (j * frames->size); // this frame starting point
    j++; // count of data frames done
    ab->head.data = (*start) & 0b10000000; // stash copy of first bit from data frame
    ab->head.state = AB_UNKNOWN;

    // last frame special handling
    if(j * frames->size >= seq->len)
    {
      ab->head.last = true;
      uint16_t rem = seq->len % frames->size;
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

  // overall sequence checkhash
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
  
  if(!out)
  {
    sequence_t s = (frames->sending)?frames->sending:frames->receiving;
    if(!s) {
      LOG_DEBUG("reset flush requested");
      frames->do_reset = true;
      return frames;
    }
    if(s->is_paused) return LOG_DEBUG("paused");
    LOG_WARN("TODO remove any hold bits on frames->sending");
    s->do_flush = 1;
    return frames;
  }

  // queue and kick-start sequence if necessary
  if(frames->outbox) return LOG_DEBUG("outbox full");
  frames->outbox = out;
  if(!frames->sending) seq_out(frames);

  return frames;
}

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames)
{
  if(!frames) return NULL;
  lob_t pkt = frames->inbox;
  frames->inbox = NULL;
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

  len += frames->count_in * frames->size;

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

  size_t sent = frames->count_out * frames->size;
  len -= (sent < len)?sent:len;

  return len;
}

// is a frame pending to be sent immediately
util_frames_t util_frames_pending(util_frames_t frames)
{
  if(!frames) return LOG_WARN("bad args");
  if(frames->flush_in || frames->flush_out) return frames;
  if(!frames->sending) return LOG_CRAZY("empty");

  sequence_t s = frames->sending;
  for(uint8_t i = 0; i < frames->per; i = abs_next(s, i)) {
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

    // see if it's their sequence frame first
    murmur(frames->theirs, raw + 4, frames->size - 4, hash);
    hash[0] |= 0b10000000;
    if(memcmp(raw, hash, 4) == 0) {
      LOG_WARN("TODO their sequence frame");
      return frames;
    }

    // can't receive our sequence frame if not sending
    if(!frames->sending) return LOG_INFO("possible sequence when not sending");

    // now process our sequence frame, swap out hash to mirror sender's copy
    uint8_t orig[4];
    memcpy(orig, raw, 4);
    memcpy(raw,frames->sending->hash,4);
    murmur(frames->ours, raw, frames->size, hash);
    hash[0] |= 0b10000000;
    if(memcmp(raw, hash, 4) != 0) return LOG_INFO("possible sequence checkhash failed");

    LOG_WARN("TODO our sequence frame");
    return frames;
  }

  // do we know how to receive data yet?
  if(!frames->receiving) return LOG_INFO("possible data frame but no receiving sequence");

  // is a data frame, check both possible hashes
  uint8_t zero[4], one[4];
  murmur(frames->theirs, raw, frames->size, zero);
  raw[0] |= 0b10000000;
  murmur(frames->theirs, raw, frames->size, one);

  sequence_t s = frames->receiving;
  bool all = true;
  for(uint8_t i = 0, j = 0; i < frames->per; i = abs_next(s, i)) {
    abstract_t ab = &(s->abs[i]);
    if(!abs_isframe(ab)) continue;
    uint8_t *start = frames->at + (j * frames->size); // this frame starting point
    j++;
    uint8_t *hash = (ab->head.data)?one:zero;
    if(memcmp(ab->hash, (ab->head.data) ? one : zero) != 0)
    {
      // track if all have been received so far
      if(ab->head.state != AB_RECEIVED) all = false;
      continue;
    }
    
    if(ab->head.state == AB_RECEIVED)
    {
      LOG_DEBUG("ignoring duplicate data frame");
      return frames;
    }

    // handle last frame partial
    uint16_t len = frames->size;
    if(ab->head.end)
    {
      uint16_t rem = frames->len % frames->size;
      if(rem) len = rem;
    }

    // reject a request to write past end of packet
    if(start+len > lob_raw(frames->in) + lob_len(frames->in))
    {
      ab->head.hold = 3; // packet rejection/restart signal
      frames->flush_in = true;
      return LOG_WARN("rejecting data past end of current packet");
    }

    frames->count_in++;
    LOG_CRAZY("we've got ourselves a data frame len %u total %u", len, frames->count_in);
    // above raw[0] bit was set to 1, set back to zero if necessary
    if(!ab->head.data) raw[0] &= 0b01111111;
    memcpy(start, raw, len);
    ab->head.state = AB_RECEIVED;

    // if packet complete, party!
    if(ab->head.last && all)
    {
      LOG_DEBUG("last frame received, packet done");
      frames->flush_in = true;
      frames->inbox = lob_push(frames->inbox, frames->in);
      frames->in = NULL;
      frames->at = NULL;
      frames->count_in = 0;
    }

    return frames;
  }

  return LOG_INFO("checkhash failed on possible data frame");
}

util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data)
{
  if(!frames) return LOG_WARN("bad args");
  if(!data) return util_frames_pending(frames); // just a ready check

  if(frames->flush_in) {
    // send frames->receiving sequence frame
    frames->flush_in = false;
    return frames;
  }

  if(frames->flush_out) {
    // send frames->sending sequence frame
    // fills in empty sequence frame if not sending
    frames->flush_out = false;
    return frames;
  }

  if(!frames->sending) return LOG_DEBUG("nothing to send");

  LOG_DEBUG("looking for a data frame")
  sequence_t s = frames->sending;
  for(uint8_t i = 0, j = 0; i < frames->per; i = abs_next(s, i)) {
    abstract_t ab = &(s->abs[i]);
    if(!abs_isframe(ab)) continue;
    uint8_t *start = frames->out + (j * frames->size); // this frame starting point
    j++;
    if(ab->head.state == AB_SENT) continue;
    if(ab->head.state == AB_RECEIVED) continue;
    if(ab->head.hold) return LOG_DEBUG("hold requested, flush to continue");

    // got one
    uint16_t len = frames->size;
    if(ab->head.end) {
      uint16_t rem = frames->len % frames->size;
      if(rem) len = rem;
    }
    LOG_DEBUG("sending data frame %u len %u",i,len);
    memcpy(out,start,len);
    return frames;
  }

  return LOG_DEBUG("no more data frames to send");
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

