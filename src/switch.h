#ifndef switch_h
#define switch_h

#include "hn.h"
#include "dht.h"
#include "packet.h"

typedef struct switch_struct
{
  hn_t id;
  dht_t seeds, all;
  packet_t out, last;
} *switch_t;

switch_t switch_new(hn_t id);
void switch_free(switch_t s);

// add hashname as a seed, will automatically trigger a query to it
void switch_seed(switch_t s, hn_t h);

// get a packet to be sent, NULL if none
packet_t switch_sending(switch_t s);

// add a packet to the send queue
void switch_send(switch_t s, packet_t p);

#endif