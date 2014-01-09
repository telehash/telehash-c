#include "switch.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
  
switch_t switch_new(hn_t id)
{
  switch_t s = malloc(sizeof (struct switch_struct));
  bzero(s, sizeof(struct switch_struct));
  s->id = id;
  return s;
}

void switch_free(switch_t s)
{
  if(s->seeds) dht_free(s->seeds);
  if(s->all) dht_free(s->all);
  free(s);
}

void switch_seed(switch_t s, hn_t h)
{
  if(!s->seeds) s->seeds = dht_new();
  dht_add(s->seeds, h);
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
