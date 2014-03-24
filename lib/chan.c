#include "chan.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "switch.h"

// link c to hn
void chan_hn(hn_t hn, chan_t c)
{
  if(!hn || !c) return;
  if(!hn->chans) hn->chans = xht_new(17);
  xht_set(hn->chans,(char*)c->hexid,c);
}

void chan_reset(switch_t s, hn_t to)
{
  // TODO, fail any existing hn->chans from the sender
  if(!to->chanOut) to->chanOut = (strncmp(s->id->hexname,to->hexname,64) > 0) ? 1 : 2;
}

chan_t chan_new(switch_t s, hn_t to, char *type, int reliable)
{
  chan_t c;
  if(!s || !to || !type) return NULL;

  if(!to->chanOut) chan_reset(s, to);
  c = malloc(sizeof (struct chan_struct));
  memset(c,0,sizeof (struct chan_struct));
  c->type = strdup(type);
  c->s = s;
  c->to = to;
  c->reliable = reliable;
  c->state = STARTING;
  c->id = to->chanOut;
  to->chanOut += 2;
  util_hex((unsigned char*)&(c->id),4,(unsigned char*)c->hexid);
  chan_hn(to, c);
  return c;
}

chan_t chan_in(switch_t s, hn_t from, packet_t p)
{
  chan_t c;
  unsigned long id;
  char hexid[9], *type;
  if(!from || !p) return NULL;

  id = strtol(packet_get_str(p,"c"), NULL, 10);
  util_hex((unsigned char*)&id,4,(unsigned char*)hexid);
  c = xht_get(from->chans, hexid);
  if(c) return c;

  type = packet_get_str(p, "type");
  DEBUG_PRINTF("channel new %d %s\n",id,type);
  if(!type || id % 2 == from->chanOut % 2) return NULL;

  c = malloc(sizeof (struct chan_struct));
  memset(c,0,sizeof (struct chan_struct));
  c->type = strdup(type);
  c->s = s;
  c->to = from;
  c->reliable = packet_get_str(p,"seq")?1:0;
  c->state = STARTING;
  c->id = id;
  util_hex((unsigned char*)&(c->id),4,(unsigned char*)c->hexid);
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
  packet_set_int(p,"c",c->id);
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
  DEBUG_PRINTF("channel in %d %.*s\n",c->id,p->json_len,p->json);
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