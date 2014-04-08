#include "chan.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "switch.h"

// link c to hn
void chan_hn(hn_t hn, chan_t c)
{
  if(!hn || !c) return;
}

void walkend(xht_t h, const char *key, void *val, void *arg)
{
  uint8_t base = *(uint8_t*)arg;
  chan_t c = (chan_t)val;
  if(c->id % 2 != base % 2) chan_fail(c,NULL);
}
void chan_reset(switch_t s, hn_t to)
{
  uint8_t base = (strncmp(s->id->hexname,to->hexname,64) > 0) ? 1 : 2;
  if(!to->chanOut) to->chanOut = base;
  // fail any existing chans from them
  xht_walk(to->chans, &walkend, (void*)&base);
}

chan_t chan_reliable(chan_t c, int window)
{
  if(!c || !window || c->state != STARTING || c->reliable) return c;
  c->reliable = window;
  chan_seq_init(c);
  chan_miss_init(c);
  return c;
}

// kind of a macro, just make a reliable channel of this type
chan_t chan_start(switch_t s, char *hn, char *type)
{
  chan_t c;
  if(!s || !hn) return NULL;
  c = chan_new(s, hn_gethex(s->index,hn), type, 0);
  return chan_reliable(c, s->window);
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
  util_hex((unsigned char*)&(s->uid),4,(unsigned char*)c->uid); // switch-wide unique id
  s->uid++;
  util_hex((unsigned char*)&(c->id),4,(unsigned char*)c->hexid);
  if(!to->chans) to->chans = xht_new(17);
  xht_set(to->chans,(char*)c->hexid,c);
  xht_set(s->index,(char*)c->uid,c);
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

// flags channel as ended, optionally adds end to packet
chan_t chan_end(chan_t c, packet_t p)
{
  DEBUG_PRINTF("channel end %d",c->id);
  if(p) packet_set(p,"end","true",4);
  // if(c->reliable) TODO set to ENDING, add timer for cleanup and then queue for free
  c->state = ENDED;
  chan_queue(c);
  return c;
}

// immediately fails/removes channel, if err tries to send message
chan_t chan_fail(chan_t c, char *err)
{
  packet_t e;
  DEBUG_PRINTF("channel fail %d %s",c->id,err);
  if(err && c->state != ENDED && (e = chan_packet(c)))
  {
    packet_set_str(e,"err",err);
    chan_send(c,e);
  }
  // no grace period for reliable
  c->state = ENDED;
  xht_set(c->to->chans,(char*)c->hexid,NULL);
  chan_queue(c);
  return c;
}

void chan_free(chan_t c)
{
  packet_t p;
  // remove references
  DEBUG_PRINTF("channel free %d",c->id);
  chan_dequeue(c);
  if(xht_get(c->to->chans,(char*)c->hexid) == c) xht_set(c->to->chans,(char*)c->hexid,NULL);
  xht_set(c->s->index,(char*)c->uid,NULL);
  if(c->reliable)
  {
    chan_seq_free(c);
    chan_miss_free(c);
  }
  while(c->in)
  {
    DEBUG_PRINTF("unused packets on channel %d",c->id);
    p = c->in;
    c->in = p->next;
    packet_free(p);
  }
  while(c->notes)
  {
    DEBUG_PRINTF("unused notes on channel %d",c->id);
    p = c->notes;
    c->notes = p->next;
    packet_free(p);
  }
  free(c->type);
  free(c);
}

// get the next incoming note waiting to be handled
packet_t chan_notes(chan_t c)
{
  packet_t note;
  if(!c) return NULL;
  note = c->notes;
  if(note) c->notes = note->next;
  return note;
}

// create a new note tied to this channel
packet_t chan_note(chan_t c, packet_t note)
{
  if(!note) note = packet_new();
  packet_set_str(note,".from",(char*)c->uid);
  return note;
}

// send this note back to the sender
int chan_reply(chan_t c, packet_t note)
{
  char *from;
  if(!c || !(from = packet_get_str(note,".from"))) return -1;
  packet_set_str(note,".to",from);
  packet_set_str(note,".from",(char*)c->uid);
  return switch_note(c->s,note);
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

// add to processing queue
void chan_queue(chan_t c)
{
  chan_t step;
  // add to switch queue
  step = c->s->chans;
  if(c->next || step == c) return;
  while(step && (step = step->next)) if(step == c) return;
  c->next = c->s->chans;
  c->s->chans = c;
}

// remove from processing queue
void chan_dequeue(chan_t c)
{
  chan_t step = c->s->chans;
  if(step == c)
  {
    c->s->chans = c->next;
    c->next = NULL;
    return;
  }
  step = c->s->chans;
  while(step) step = (step->next == c) ? c->next : step->next;
  c->next = NULL;
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

  // queue for processing
  chan_queue(c);
}

// smartly send based on what type of channel we are
void chan_send(chan_t c, packet_t p)
{
  if(!p) return;
  if(!c) return (void)packet_free(p);
  DEBUG_PRINTF("channel out %d %.*s",c->id,p->json_len,p->json);
  if(c->reliable) p = packet_copy(p); // miss tracks the original p = chan_packet()
  switch_send(c->s,p);
}

// optionally sends reliable channel ack-only if needed
void chan_ack(chan_t c)
{
  packet_t p;
  if(!c || !c->reliable) return;
  p = chan_seq_ack(c,NULL);
  if(!p) return;
  DEBUG_PRINTF("channel ack %d %.*s",c->id,p->json_len,p->json);
  switch_send(c->s,p);
}