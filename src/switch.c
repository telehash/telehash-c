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
  s->buckets = malloc(256 * sizeof(bucket_t));
  bzero(s->buckets, 256 * sizeof(bucket_t));
  s->index = xht_new(HNMAXPRIME);
  return s;
}

int switch_init(switch_t s, hn_t id)
{
  if(!id || !id->c) return 1;
  if(crypt_private(id->c,NULL,0)) return 1;
  s->id = id;
  return 0;
}

void switch_free(switch_t s)
{
  int i;
  for(i=0;i<=255;i++) if(s->buckets[i]) bucket_free(s->buckets[i]);
  if(s->seeds) bucket_free(s->seeds);
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
  if(!s->buckets[bucket]) s->buckets[bucket] = bucket_new();
  bucket_add(s->buckets[bucket], hn);
  // TODO figure out if there's actually more capacity
  
}

void switch_seed(switch_t s, hn_t hn)
{
  if(!s->seeds) s->seeds = bucket_new();
  bucket_add(s->seeds, hn);
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
