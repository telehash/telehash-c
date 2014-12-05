#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../lib/util.h"
#include "e3x.h"
#include "platform.h"

// every new channel has a unique global id
static uint32_t _uids = 0;

// internal only structure, always use accessors
struct channel3_struct
{
  uint32_t id; // wire id (not unique)
  char c[12]; // str of id
  char uid[9]; // process hex id (unique)
  uint32_t tsent, trecv; // last send, recv
  uint32_t timeout; // seconds since last trecv to auto-err
  lob_t open; // cached for convenience
  char *type;
  enum channel3_states state;
  event3_t ev;
  uint32_t capacity, max; // totals for windowing
  
  // reliable miss tracking
  uint32_t miss_nextack;
  lob_t out;
  
  // reliable seq tracking
  uint32_t seq, seq_nextin, seq_seen, seq_acked;
  lob_t in;
};

// open must be channel3_receive or channel3_send next yet
channel3_t channel3_new(lob_t open)
{
  uint32_t id;
  char *type;
  channel3_t c;

  if(!open) return LOG("open packet required");
  id = (uint32_t)lob_get_int(open,"c");
  if(!id) return LOG("invalid channel id");
  type = lob_get(open,"type");
  if(!type) return LOG("missing channel type");

  c = malloc(sizeof (struct channel3_struct));
  memset(c,0,sizeof (struct channel3_struct));
  c->state = OPENING;
  c->id = id;
  sprintf(c->c,"%u",id);
  c->open = lob_copy(open);
  c->type = lob_get(open,"type");
  c->capacity = 1024*1024; // 1MB total default

  // generate a unique id in hex
  _uids++;
  util_hex((uint8_t*)&_uids,4,c->uid);

  // reliability
  if(lob_get(open,"seq"))
  {
    c->seq = 1;
  }

  LOG("new %s channel %s %d %s",c->seq?"reliable":"unreliable",c->uid,id,type);
  return c;
}

void channel3_free(channel3_t c)
{
  lob_t tmp;
  if(!c) return;
  // cancel timeouts
  channel3_timeout(c,NULL,0);
  // free cached packet
  lob_free(c->open);
  // free any other queued packets
  while(c->in)
  {
    tmp = c->in;
    c->in = tmp->next;
    lob_free(tmp);
  }
  while(c->out)
  {
    tmp = c->out;
    c->out = tmp->next;
    lob_free(tmp);
  }
  free(c);
};

// return its unique id in hex
char *channel3_uid(channel3_t c)
{
  if(!c) return NULL;
  return c->uid;
}

// return the numeric per-exchange id
uint32_t channel3_id(channel3_t c)
{
  if(!c) return 0;
  return c->id;
}

char *channel3_c(channel3_t c)
{
  if(!c) return NULL;
  return c->c;
}

// this will set the default inactivity timeout using this event timer and our uid
uint32_t channel3_timeout(channel3_t c, event3_t ev, uint32_t timeout)
{
  if(!c) return 0;

  // un-set any
  if(!ev)
  {
    // cancel any that may have been stored
    if(c->ev) event3_set(c->ev,NULL,c->uid,0);
    c->ev = NULL;
    c->timeout = 0;
    return 0;
  }
  
  // return how much time is left
  if(!timeout)
  {
    // TODO calculate it
    return c->timeout;
  }

  // add/update new timeout
  c->ev = ev;
  c->timeout = timeout;
  // TODO set up a timer
  return c->timeout;
}

// returns the open packet (always cached)
lob_t channel3_open(channel3_t c)
{
  if(!c) return NULL;
  return c->open;
}

enum channel3_states channel3_state(channel3_t c)
{
  if(!c) return ENDED;
  return c->state;
}

// incoming packets

// usually sets/updates event timer, ret if accepted/valid into receiving queue
uint8_t channel3_receive(channel3_t c, lob_t inner)
{
  lob_t end;
  if(!c || !inner) return 1;
  if(!c->in)
  {
    c->in = inner;
    return 0;
  }

  // TODO reliability

  end = c->in;
  while(end->next) end = end->next;
  end->next = inner;

  return 0;
}

// false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_sync)
void channel3_sync(channel3_t c, uint8_t sync)
{
  if(!c) return;
  LOG("%s sync %d TODO",c->uid,sync);
}

// get next avail packet in order, null if nothing
lob_t channel3_receiving(channel3_t c)
{
  lob_t ret;
  if(!c || !c->in) return NULL;
  // TODO reliability
  ret = c->in;
  c->in = ret->next;
  return ret;
}

// outgoing packets

// creates a packet w/ necessary json, just a convenience
lob_t channel3_packet(channel3_t c)
{
  lob_t ret;
  if(!c) return NULL;
  
  ret = lob_new();
  lob_set_int(ret,"c",c->id);
  // TODO reliability
  return ret;
}

// adds to sending queue, adds json if needed
uint8_t channel3_send(channel3_t c, lob_t inner)
{
  lob_t end;
  if(!c || !inner) return 1;
  
  if(!lob_get_int(inner,"c")) lob_set_int(inner,"c",c->id);

  // if reliable, add seq
  if(c->seq) lob_set_int(inner,"seq",c->seq++);

  LOG("channel send %d %s",c->id,lob_json(inner));

  if(!c->out)
  {
    c->out = inner;
    return 0;
  }

  end = c->out;
  while(!end->next) end = end->next;
  end->next = inner;

  return 0;
}

// must be called after every send or receive, pass pkt to e3x_encrypt before sending
lob_t channel3_sending(channel3_t c)
{
  lob_t ret;
  if(!c || !c->out) return NULL;
  // TODO reliability
  ret = c->out;
  c->out = ret->next;
  return ret;
}

// size (in bytes) of buffered data in or out
uint32_t channel3_size(channel3_t c, uint32_t max)
{
  uint32_t size = 0;
  lob_t cur;
  if(!c) return 0;

  // update max capacity if given
  if(max) c->capacity = max;

  // add up the sizes of the in and out buffers
  cur = c->in;
  while(cur)
  {
    size += lob_len(cur);
    cur = cur->next;
  }
  cur = c->out;
  while(cur)
  {
    size += lob_len(cur);
    cur = cur->next;
  }

  return size;
}

/*
// immediately removes channel, creates/sends packet to app to notify
void doerror(channel3_t c, lob_t p, char *err)
{
  if(c->ended) return;
  DEBUG_PRINTF("channel fail %d %s %d %d %d",c->id,err?err:lob_get_str(p,"err"),c->timeout,c->tsent,c->trecv);

  // unregister for any more packets immediately
  xht_set(c->to->chans,(char*)c->hexid,NULL);
  if(c->id > c->to->chanMax) c->to->chanMax = c->id;

  // convenience to generate error packet
  if(!p)
  {
    if(!(p = lob_new())) return;
    lob_set_str(p,"err",err?err:"unknown");
    lob_set_int(p,"c",c->id);
  }

  // send error to app immediately, will doend() when processed
  p->next = c->in;
  c->in = p;
  channel3_queue(c);
}


channel3_t channel3_reliable(channel3_t c, int window)
{
  if(!c || !window || c->tsent || c->trecv || c->reliable) return c;
  c->reliable = window;
  channel3_seq_init(c);
  channel3_miss_init(c);
  return c;
}

void channel3_timeout(channel3_t c, uint32_t seconds)
{
  if(!c) return;
  c->timeout = seconds;
}

// convenience
void channel3_end(channel3_t c, lob_t p)
{
  if(!c) return;
  if(!p && !(p = channel3_packet(c))) return;
  lob_set(p,"end","true",4);
  channel3_send(c,p);
}

// send errors directly
channel3_t channel3_fail(channel3_t c, char *err)
{
  lob_t p;
  if(!c) return NULL;
  if(!(p = lob_new())) return c;
  doend(c);
  p->to = c->to;
  lob_set_str(p,"err",err?err:"unknown");
  lob_set_int(p,"c",c->id);
  switch_send(c->s,p);
  return c;
}

// smartly send based on what type of channel we are
void channel3_send(channel3_t c, lob_t p)
{
  if(!p) return;
  if(!c) return (void)lob_free(p);
  DEBUG_PRINTF("channel out %d %.*s",c->id,p->json_len,p->json);
  c->tsent = c->s->tick;
  if(c->reliable && lob_get_str(p,"seq")) p = lob_copy(p); // miss tracks the original p = channel3_packet(), a sad hack
  if(lob_get_str(p,"err") || util_cmp(lob_get_str(p,"end"),"true") == 0) doend(c); // track sending error/end
  else if(!c->reliable && !c->opened && !c->in) c->in = lob_copy(p); // if normal unreliable start packet, track for resends
  
  switch_send(c->s,p);
}

// optionally sends reliable channel ack-only if needed
void channel3_ack(channel3_t c)
{
  lob_t p;
  if(!c || !c->reliable) return;
  p = channel3_seq_ack(c,NULL);
  if(!p) return;
  DEBUG_PRINTF("channel ack %d %.*s",c->id,p->json_len,p->json);
  switch_send(c->s,p);
}

void channel3_seq_init(channel3_t c)
{
  seq_t s = malloc(sizeof (struct seq_struct));
  memset(s,0,sizeof (struct seq_struct));
  s->in = malloc(sizeof (lob_t) * c->reliable);
  memset(s->in,0,sizeof (lob_t) * c->reliable);
  c->seq = (void*)s;
}

void channel3_seq_free(channel3_t c)
{
  int i;
  seq_t s = (seq_t)c->seq;
  for(i=0;i<c->reliable;i++) lob_free(s->in[i]);
  free(s->in);
  free(s);
}

// add ack, miss to any packet
lob_t channel3_seq_ack(channel3_t c, lob_t p)
{
  char *miss;
  int i,max;
  seq_t s = (seq_t)c->seq;

  // determine if we need to ack
  if(!s->nextin) return p;
  if(!p && s->acked && s->acked == s->nextin-1) return NULL;

  if(!p)
  {
    if(!(p = lob_new())) return NULL;
    p->to = c->to;
    lob_set_int(p,"c",c->id);
    DEBUG_PRINTF("making SEQ only");
  }
  s->acked = s->nextin-1;
  lob_set_int(p,"ack",(int)s->acked);

  // check if miss is not needed
  if(s->seen < s->nextin || s->in[0]) return p;
  
  // create miss array, c sux
  max = (c->reliable < 10) ? c->reliable : 10;
  miss = malloc(3+(max*11)); // up to X uint32,'s
  memcpy(miss,"[\0",2);
  for(i=0;i<max;i++) if(!s->in[i]) sprintf(miss+strlen(miss),"%d,",(int)s->nextin+i);
  if(miss[strlen(miss)-1] == ',') miss[strlen(miss)-1] = 0;
  memcpy(miss+strlen(miss),"]\0",2);
  lob_set(p,"miss",miss,strlen(miss));
  free(miss);
  return p;
}

// new channel sequenced packet
lob_t channel3_seq_packet(channel3_t c)
{
  lob_t p = lob_new();
  seq_t s = (seq_t)c->seq;
  
  // make sure there's tracking space
  if(channel3_miss_track(c,s->id,p)) return NULL;

  // set seq and add any acks
  lob_set_int(p,"seq",(int)s->id++);
  return channel3_seq_ack(c, p);
}

// buffers packets until they're in order
int channel3_seq_receive(channel3_t c, lob_t p)
{
  int offset;
  uint32_t id;
  char *seq;
  seq_t s = (seq_t)c->seq;

  // drop or cache incoming packet
  seq = lob_get_str(p,"seq");
  id = seq?(uint32_t)strtol(seq,NULL,10):0;
  offset = id - s->nextin;
  if(!seq || offset < 0 || offset >= c->reliable || s->in[offset])
  {
    lob_free(p);
  }else{
    s->in[offset] = p;
  }

  // track highest seen
  if(id > s->seen) s->seen = id;
  
  return s->in[0] ? 1 : 0;
}

// returns ordered packets for this channel, updates ack
lob_t channel3_seq_pop(channel3_t c)
{
  lob_t p;
  seq_t s = (seq_t)c->seq;
  if(!s->in[0]) return NULL;
  // pop off the first, slide any others back, and return
  p = s->in[0];
  memmove(s->in, s->in+1, (sizeof (lob_t)) * (c->reliable - 1));
  s->in[c->reliable-1] = 0;
  s->nextin++;
  return p;
}

void channel3_miss_init(channel3_t c)
{
  miss_t m = (miss_t)malloc(sizeof (struct miss_struct));
  memset(m,0,sizeof (struct miss_struct));
  m->out = (lob_t*)malloc(sizeof (lob_t) * c->reliable);
  memset(m->out,0,sizeof (lob_t) * c->reliable);
  c->miss = (void*)m;
}

void channel3_miss_free(channel3_t c)
{
  int i;
  miss_t m = (miss_t)c->miss;
  for(i=0;i<c->reliable;i++) lob_free(m->out[i]);
  free(m->out);
  free(m);
}

// 1 when full, backpressure
int channel3_miss_track(channel3_t c, uint32_t seq, lob_t p)
{
  miss_t m = (miss_t)c->miss;
  if(seq - m->nextack > (uint32_t)(c->reliable - 1))
  {
    lob_free(p);
    return 1;
  }
  m->out[seq - m->nextack] = p;
  return 0;
}

// looks at incoming miss/ack and resends or frees
void channel3_miss_check(channel3_t c, lob_t p)
{
  uint32_t ack;
  int offset, i;
  char *id, *sack;
  lob_t miss = lob_get_packet(p,"miss");
  miss_t m = (miss_t)c->miss;

  sack = lob_get_str(p,"ack");
  if(!sack) return; // grow some
  ack = (uint32_t)strtol(sack, NULL, 10);
  // bad data
  offset = ack - m->nextack;
  if(offset < 0 || offset >= c->reliable) return;

  // free and shift up to the ack
  while(m->nextack <= ack)
  {
    // TODO FIX, refactor check into two stages
//    lob_free(m->out[0]);
    memmove(m->out,m->out+1,(sizeof (lob_t)) * (c->reliable - 1));
    m->out[c->reliable-1] = 0;
    m->nextack++;
  }

  // track any miss packets if we have them and resend
  if(!miss) return;
  for(i=0;(id = lob_get_istr(miss,i));i++)
  {
    ack = (uint32_t)strtol(id,NULL,10);
    offset = ack - m->nextack;
    if(offset >= 0 && offset < c->reliable && m->out[offset]) switch_send(c->s,lob_copy(m->out[offset]));
  }
}

// resends all
void channel3_miss_resend(channel3_t c)
{
  int i;
  miss_t m = (miss_t)c->miss;
  for(i=0;i<c->reliable;i++) if(m->out[i]) switch_send(c->s,lob_copy(m->out[i]));
}

*/