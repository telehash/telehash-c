#ifndef switch_h
#define switch_h

#include "hn.h"
#include "bucket.h"
#include "packet.h"
#include "xht.h"
#include "chan.h"
#include "util.h"
#include "platform.h"
#include "crypt.h"
#include "path.h"

typedef struct switch_struct
{
  hn_t id;
  bucket_t seeds;
  packet_t out, last; // packets waiting to be delivered
  packet_t parts;
  chan_t chans; // channels waiting to be processed
  uint32_t uid;
  int cap, window;
  xht_t index;
} *switch_t;

switch_t switch_new();
void switch_free(switch_t s);

// must be called to initialize to a hashname from json keys, return !0 if failed, free's keys
int switch_init(switch_t s, packet_t keys);

// add hashname as a seed, will automatically trigger a query to it
void switch_seed(switch_t s, hn_t hn);

// should be called at least once a second, handles timers
void switch_loop(switch_t s);

// get a packet to be sent, NULL if none
packet_t switch_sending(switch_t s);

// get a channel that has packets to be processed, NULL if none
chan_t switch_pop(switch_t s);

// encrypts a packet and adds it to the sending queue, mostly internal use
void switch_send(switch_t s, packet_t p);

// adjust/set the cap of how many lines to maintain (256) and reliable window packet buffer size (32)
void switch_capwin(switch_t s, int cap, int window);

// processes incoming packet, it will free p, leaves in alone (is reusable)
void switch_receive(switch_t s, packet_t p, path_t in);

// tries to send an open (if we need to), mostly internal
void switch_open(switch_t s, hn_t to, path_t direct);

// sends a note packet to it's channel if it can, !0 for error
int switch_note(switch_t s, packet_t note);

#endif