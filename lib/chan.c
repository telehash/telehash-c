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

chan_t chan_reliable(chan_t c, int window)
{
  if(!c || !window || c->state != STARTING) return c;
  c->reliable = window;
  chan_seq_init(c);
  chan_miss_init(c);
  return c;
}

chan_t chan_new(switch_t s, hn_t to, char *type, uint32_t id)
{
  chan_t c;
  if(!s || !to || !type) return NULL;

  // use new id if none given
  if(!to->chanOut) chan_reset(s, to);
  if(!id)
  {
    id = to->chanOut;
    to->chanOut += 2;
  }

  DEBUG_PRINTF("channel new %d %s",id,type);
  c = malloc(sizeof (struct chan_struct));
  memset(c,0,sizeof (struct chan_struct));
  c->type = strdup(type);
  c->s = s;
  c->to = to;
  c->state = STARTING;
  c->id = id;
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
  if(!type || id % 2 == from->chanOut % 2) return NULL;

  return chan_new(s, from, type, id);
}

void chan_free(chan_t c)
{
  xht_set(c->to->chans,(char*)c->hexid,NULL);
  if(c->reliable)
  {
    chan_seq_free(c);
    chan_miss_free(c);
  }
  free(c->type);
  free(c);
}

// create a packet ready to be sent for this channel
packet_t chan_packet(chan_t c)
{
  packet_t p;
  if(!c || c->state == ENDED) return NULL;
  p = c->reliable?chan_seq_packet(c):packet_new();
  if(!p) return NULL;
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
  if(!c) return NULL;
  if(c->reliable) return chan_seq_pop(c);
  if(!c->in) return NULL;
  p = c->in;
  c->in = p->next;
  if(!c->in) c->inend = NULL;
  return p;
}

// internal, receives/processes incoming packet
void chan_receive(chan_t c, packet_t p)
{
  if(!c || !p) return;
  DEBUG_PRINTF("channel in %d %.*s",c->id,p->json_len,p->json);
  if(c->state == ENDED) return (void)packet_free(p);
  if(c->state == STARTING) c->state = OPEN;
  if(util_cmp(packet_get_str(p,"end"),"true") == 0) c->state = ENDING;
  if(packet_get_str(p,"err")) c->state = ENDED;

  if(c->reliable)
  {
    chan_miss_check(c,p);
    if(!chan_seq_receive(c,p)) return; // queued, nothing to do
  }else{
    // add to the end of the raw packet queue
    if(c->inend)
    {
      c->inend->next = p;
      c->inend = p;
      return;
    }
    c->inend = c->in = p;    
  }
  
  // add to channels queue, if not
  if(c->next || c->s->chans == c) return;
  c->next = c->s->chans;
  c->s->chans = c;
}