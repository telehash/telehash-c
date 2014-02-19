#ifndef switch_h
#define switch_h

#include "hn.h"
#include "bucket.h"
#include "packet.h"
#include "xht.h"
#include "chan.h"

typedef struct switch_struct
{
  hn_t id;
  bucket_t seeds, *buckets;
  packet_t out, last; // packets waiting to be delivered
  chan_t chans; // channels waiting to be processed
  int cap;
  xht_t index;
} *switch_t;

switch_t switch_new();
void switch_free(switch_t s);

// must be called to initialize to a hashname from json keys, return !0 if failed, free's keys
int switch_init(switch_t s, packet_t keys);

// add hashname as a seed, will automatically trigger a query to it
void switch_seed(switch_t s, hn_t hn);

// get a packet to be sent, NULL if none
packet_t switch_sending(switch_t s);

// get a channel that has packets to be processed, NULL if none
chan_t switch_pop(switch_t s);

// encrypts a packet and adds it to the sending queue
void switch_send(switch_t s, packet_t p);

// adjust/set the cap of how many lines to maintain, defaults to 256
void switch_cap(switch_t s, int cap);

// processes incoming packet, it will free p
void switch_receive(switch_t s, packet_t p, path_t in);

// tries to send an open (if we need to), mostly internal
void switch_open(switch_t s, hn_t to, path_t direct);

#endif