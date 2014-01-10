#include "switch.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

// a prime number for the internal hashtable used to track all active hashnames
#define HNMAXPRIME 4211

switch_t switch_new(hn_t id)
{
  switch_t s = malloc(sizeof (struct switch_struct));
  bzero(s, sizeof(struct switch_struct));
  s->id = id;
  s->cap = 256; // default cap size
  // create all the buckets
  s->buckets = malloc(256 * sizeof(hnt_t));
  bzero(s->buckets, 256 * sizeof(hnt_t));
  s->index = xht_new(HNMAXPRIME);
  return s;
}

void switch_free(switch_t s)
{
  int i;
  for(i=0;i<=255;i++) if(s->buckets[i]) hnt_free(s->buckets[i]);
  if(s->seeds) hnt_free(s->seeds);
  free(s);
}

void switch_cap(switch_t s, int cap)
{
  s->cap = cap;
}

// add this hashname to our bucket list
void switch_bucket(switch_t s, hn_t hn)
{
  unsigned char bucket = hn_distance(s->id, hn);
  if(!s->buckets[bucket]) s->buckets[bucket] = hnt_new();
  hnt_add(s->buckets[bucket], hn);
  // TODO figure out if there's actually more capacity
  
}

void switch_seed(switch_t s, hn_t hn)
{
  if(!s->seeds) s->seeds = hnt_new();
  hnt_add(s->seeds, hn);
}

packet_t switch_sending(switch_t s)
{
  packet_t p;
  if(!s->out) return NULL;
  p = s->out;
  s->out = p->next;
  if(!s->out) s->last = NULL;
  return p;
}

void switch_send(switch_t s, packet_t p)
{
  if(s->last)
  {
    s->last->next = p;
    return;
  }
  s->last = s->out = p;
}

chan_t switch_pop(switch_t s)
{
  chan_t c;
  if(!s->in) return NULL;
  c = s->in;
  s->in = c->next;
  return c;
}
