#include "chan.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "util.h"
#include "switch.h"

// link c to hn
void chan_hn(hn_t hn, chan_t c)
{
  if(!hn || !c) return;
  if(!hn->chans) hn->chans = xht_new(17);
  xht_set(hn->chans,c->hexid,c);
}

chan_t chan_new(switch_t s, hn_t to, char *type, int reliable)
{
  chan_t c;
  if(!s || !to || !type) return NULL;

  c = malloc(sizeof (struct chan_struct));
  bzero(c,sizeof (struct chan_struct));
  c->type = strdup(type);
  c->s = s;
  c->to = to;
  c->reliable = reliable;
  c->state = STARTING;
  crypt_rand(c->id, 16);
  util_hex(c->id,16,(unsigned char*)c->hexid);
  chan_hn(to, c);
  return c;
}

chan_t chan_in(switch_t s, hn_t from, packet_t p)
{
  chan_t c;
  char *id, *type;
  if(!from || !p) return NULL;

  id = packet_get_str(p, "c");
  c = xht_get(from->chans, id);
  if(c) return c;

  type = packet_get_str(p, "type");
  if(!id || strlen(id) != 32 || !type) return NULL;

  c = malloc(sizeof (struct chan_struct));
  bzero(c,sizeof (struct chan_struct));
  c->type = strdup(type);
  c->s = s;
  c->to = from;
  c->reliable = packet_get_str(p,"seq")?1:0;
  c->state = STARTING;
  util_unhex((unsigned char*)id,32,c->id);
  util_hex(c->id,16,(unsigned char*)c->hexid);
  chan_hn(from, c);
  return c;
}

void chan_free(chan_t c)
{
  // remove from hn
  free(c->type);
  free(c);
}

// create a packet ready to be sent for this channel
packet_t chan_packet(chan_t c)
{
  packet_t p;
  if(!c || c->state == ENDED) return NULL;
  p = packet_new();
  p->to = c->to;
  if(path_alive(c->last)) p->out = c->last;
  if(c->state == STARTING)
  {
    packet_set_str(p,"type",c->type);
  }
  packet_set_str(p,"c",c->hexid);
  return p;
}

packet_t chan_pop(chan_t c)
{
  packet_t p;
  if(!c || !c->in) return NULL;
  p = c->in;
  c->in = p->next;
  if(!c->in) c->inend = NULL;
  return p;
}

// internal, receives/processes incoming packet
void chan_receive(chan_t c, packet_t p)
{
  if(!c || !p) return;
  if(c->state == ENDED) return (void)packet_free(p);
  if(c->state == STARTING) c->state = OPEN;
  if(util_cmp(packet_get_str(p,"end"),"true") == 0) c->state = ENDED;
  if(packet_get_str(p,"err")) c->state = ENDED;

  // add to the end of the queue
  if(c->inend)
  {
    c->inend->next = p;
    c->inend = p;
    return;
  }
  c->inend = c->in = p;
  
  // add to channels queue, if not
  if(c->next || c->s->chans == c) return;
  c->next = c->s->chans;
  c->s->chans = c;
}