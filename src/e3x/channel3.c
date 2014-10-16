#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "e3x.h"
#include "platform.h"

// internal only structure, always use accessors
struct channel3_struct
{
  uint32_t id; // wire id (not unique)
  lob_t open; // initial open cached
  uint32_t tsent, trecv; // last send, recv
  uint32_t timeout; // seconds since last trecv to auto-err
  char uid[9]; // uid is switch-wide unique
  char *type;
  uint8_t reliable;
  uint8_t state;
  lob_t in, inend, notes;
  void *arg; // used by extensions
  void *seq, *miss; // used by channel3_seq/channel3_miss
  // event callbacks for channel implementations
  void (*handler)(struct channel3_struct*); // handle incoming packets immediately/directly
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

  LOG("channel new %d %s",id,type);
  c = malloc(sizeof (struct channel3_struct));
  memset(c,0,sizeof (struct channel3_struct));
  c->open = lob_copy(open);
  c->id = id;
  c->type = lob_get(c->open,"type");
  return c;
}

void channel3_free(channel3_t c)
{
  if(!c) return;
  free(c);
};

void channel3_ev(channel3_t c, event3_t ev){}; // timers only work with this set

// incoming packets
uint8_t channel3_receive(channel3_t c, lob_t inner){return 0;}; // usually sets/updates event timer, ret if accepted/valid into receiving queue
void channel3_sync(channel3_t c, uint8_t sync){}; // false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_sync)
lob_t channel3_receiving(channel3_t c){return NULL;}; // get next avail packet in order, null if nothing

// outgoing packets
lob_t channel3_packet(channel3_t c){return NULL;};  // creates a packet w/ necessary json, just a convenience
uint8_t channel3_send(channel3_t c, lob_t inner){return 0;}; // adds to sending queue, adds json if needed
lob_t channel3_sending(channel3_t c){return 0;}; // must be called after every send or receive, pass pkt to e3x_encrypt before sending

// convenience functions
uint32_t channel3_id(channel3_t c){return 0;}; // numeric of the open->cid
lob_t channel3_open(channel3_t c){return NULL;}; // returns the open packet (always cached)
uint8_t channel3_state(channel3_t c){return 0;}; // E3CHAN_OPENING, E3CHAN_OPEN, or E3CHAN_ENDED
uint32_t channel3_size(channel3_t c){return 0;}; // size (in bytes) of buffered data in or out

/*
// default channel inactivity timeout in seconds
#define CHAN_TIMEOUT 10

struct channel3_struct
{
  uint32_t id; // wire id (not unique)
  uint32_t tsent, trecv; // tick value of last send, recv
  uint32_t timeout; // seconds since last trecv to auto-err
  unsigned char hexid[9], uid[9]; // uid is switch-wide unique
  char *type;
  int reliable;
  uint8_t opened, ended;
  uint8_t tfree, tresend; // tick counts for thresholds
  struct channel3_struct *next;
  lob_t in, inend, notes;
  void *arg; // used by extensions
  void *seq, *miss; // used by channel3_seq/channel3_miss
  // event callbacks for channel implementations
  void (*handler)(struct channel3_struct*); // handle incoming packets immediately/directly
  void (*tick)(struct channel3_struct*); // called approx every tick (1s)
};

// flags channel as ended either in or out
void doend(channel3_t c)
{
  if(!c || c->ended) return;
  DEBUG_PRINTF("channel ended %d",c->id);
  c->ended = 1;
}

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

void channel3_free(channel3_t c)
{
  lob_t p;
  if(!c) return;

  // if there's an arg set, we can't free and must notify channel handler
  if(c->arg) return channel3_queue(c);

  DEBUG_PRINTF("channel free %d",c->id);

  // unregister for packets (if registered)
  if(xht_get(c->to->chans,(char*)c->hexid) == (void*)c)
  {
    xht_set(c->to->chans,(char*)c->hexid,NULL);
    // to prevent replay of this old id
    if(c->id > c->to->chanMax) c->to->chanMax = c->id;
  }
  xht_set(c->s->index,(char*)c->uid,NULL);

  // remove references
  channel3_dequeue(c);
  if(c->reliable)
  {
    channel3_seq_free(c);
    channel3_miss_free(c);
  }
  while(c->in)
  {
    DEBUG_PRINTF("unused packets on channel %d",c->id);
    p = c->in;
    c->in = p->next;
    lob_free(p);
  }
  while(c->notes)
  {
    DEBUG_PRINTF("unused notes on channel %d",c->id);
    p = c->notes;
    c->notes = p->next;
    lob_free(p);
  }
  // TODO, if it's the last channel, bucket_rem(c->s->active, c);
  free(c->type);
  free(c);
}

void walkreset(xht_t h, const char *key, void *val, void *arg)
{
  channel3_t c = (channel3_t)val;
  if(!c) return;
  if(c->opened) return doerror(c,NULL,"reset");
  // flush any waiting packets
  if(c->reliable) channel3_miss_resend(c);
  else switch_send(c->s,lob_copy(c->in));
}
void channel3_reset(switch_t s, hashname_t to)
{
  // fail any existing open channels
  xht_walk(to->chans, &walkreset, NULL);
  // reset max id tracking
  to->chanMax = 0;
}

void walktick(xht_t h, const char *key, void *val, void *arg)
{
  uint32_t last;
  channel3_t c = (channel3_t)val;

  // latent cleanup/free
  if(c->ended && (c->tfree++) > c->timeout) return channel3_free(c);

  // any channel can get tick event
  if(c->tick) c->tick(c);

  // these are administrative/internal channels (to our own hashname)
  if(c->to == c->s->id) return;

  // do any auto-resend packet timers
  if(c->tresend)
  {
    // TODO check packet resend timers, channel3_miss_resend or c->in
  }

  // unreliable timeout, use last received unless none, then last sent
  if(!c->reliable)
  {
    last = (c->trecv) ? c->trecv : c->tsent;
    if(c->timeout && (c->s->tick - last) > c->timeout) doerror(c,NULL,"timeout");
  }

}

void channel3_tick(switch_t s, hashname_t hn)
{
  xht_walk(hn->chans, &walktick, NULL);
}

channel3_t channel3_reliable(channel3_t c, int window)
{
  if(!c || !window || c->tsent || c->trecv || c->reliable) return c;
  c->reliable = window;
  channel3_seq_init(c);
  channel3_miss_init(c);
  return c;
}

// kind of a macro, just make a reliable channel of this type
channel3_t channel3_start(switch_t s, char *hn, char *type)
{
  channel3_t c;
  if(!s || !hn) return NULL;
  c = channel3_new(s, hashname_gethex(s->index,hn), type, 0);
  return channel3_reliable(c, s->window);
}

channel3_t channel3_new(switch_t s, hashname_t to, char *type, uint32_t id)
{
  channel3_t c;
  if(!s || !to || !type) return NULL;

  // use new id if none given
  if(!to->chanOut) to->chanOut = (strncmp(s->id->hexname,to->hexname,64) > 0) ? 1 : 2;
  if(!id)
  {
    id = to->chanOut;
    to->chanOut += 2;
  }else{
    // externally given id can't be ours
    if(id % 2 == to->chanOut % 2) return NULL;
    // must be a newer id
    if(id <= to->chanMax) return NULL;
  }

  DEBUG_PRINTF("channel new %d %s %s",id,type,to->hexname);
  c = malloc(sizeof (struct channel3_struct));
  memset(c,0,sizeof (struct channel3_struct));
  c->type = malloc(strlen(type)+1);
  memcpy(c->type,type,strlen(type)+1);
  c->s = s;
  c->to = to;
  c->timeout = CHAN_TIMEOUT;
  c->id = id;
  util_hex((unsigned char*)&(s->uid),4,(unsigned char*)c->uid); // switch-wide unique id
  s->uid++;
  util_hex((unsigned char*)&(c->id),4,(unsigned char*)c->hexid);
  // first one on this hashname
  if(!to->chans)
  {
    to->chans = xht_new(17);
    bucket_add(s->active, to);
  }
  xht_set(to->chans,(char*)c->hexid,c);
  xht_set(s->index,(char*)c->uid,c);
  return c;
}

void channel3_timeout(channel3_t c, uint32_t seconds)
{
  if(!c) return;
  c->timeout = seconds;
}

channel3_t channel3_in(switch_t s, hashname_t from, lob_t p)
{
  channel3_t c;
  unsigned long id;
  char hexid[9];
  if(!from || !p) return NULL;

  id = strtol(lob_get_str(p,"c"), NULL, 10);
  util_hex((unsigned char*)&id,4,(unsigned char*)hexid);
  c = xht_get(from->chans, hexid);
  if(c) return c;

  return channel3_new(s, from, lob_get_str(p, "type"), id);
}

// get the next incoming note waiting to be handled
lob_t channel3_notes(channel3_t c)
{
  lob_t note;
  if(!c) return NULL;
  note = c->notes;
  if(note) c->notes = note->next;
  return note;
}

// create a new note tied to this channel
lob_t channel3_note(channel3_t c, lob_t note)
{
  if(!note) note = lob_new();
  lob_set_str(note,".from",(char*)c->uid);
  return note;
}

// send this note back to the sender
int channel3_reply(channel3_t c, lob_t note)
{
  char *from;
  if(!c || !(from = lob_get_str(note,".from"))) return -1;
  lob_set_str(note,".to",from);
  lob_set_str(note,".from",(char*)c->uid);
  return switch_note(c->s,note);
}

// create a packet ready to be sent for this channel
lob_t channel3_packet(channel3_t c)
{
  lob_t p;
  if(!c) return NULL;
  p = c->reliable?channel3_seq_packet(c):lob_new();
  if(!p) return NULL;
  p->to = c->to;
  if(!c->tsent && !c->trecv)
  {
    lob_set_str(p,"type",c->type);
  }
  lob_set_int(p,"c",c->id);
  return p;
}

lob_t channel3_pop(channel3_t c)
{
  lob_t p;
  if(!c) return NULL;
  // this allows errors to be processed immediately
  if((p = c->in))
  {
    c->in = p->next;
    if(!c->in) c->inend = NULL;    
  }
  if(!p && c->reliable) p = channel3_seq_pop(c);

  // change state as soon as an err/end is processed
  if(lob_get_str(p,"err") || util_cmp(lob_get_str(p,"end"),"true") == 0) doend(c);
  return p;
}

// add to processing queue
void channel3_queue(channel3_t c)
{
  channel3_t step;
  // add to switch queue
  step = c->s->chans;
  if(c->next || step == c) return;
  while(step && (step = step->next)) if(step == c) return;
  c->next = c->s->chans;
  c->s->chans = c;
}

// remove from processing queue
void channel3_dequeue(channel3_t c)
{
  channel3_t step = c->s->chans;
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
void channel3_receive(channel3_t c, lob_t p)
{
  if(!c || !p) return;
  DEBUG_PRINTF("channel in %d %d %.*s",c->id,p->body_len,p->json_len,p->json);
  c->trecv = c->s->tick;
  DEBUG_PRINTF("recv %d",c);
  
  // errors are immediate
  if(lob_get_str(p,"err")) return doerror(c,p,NULL);

  if(!c->opened)
  {
    DEBUG_PRINTF("channel opened %d",c->id);
    // remove any cached outgoing packet
    lob_free(c->in);
    c->in = NULL;
    c->opened = 1;
  }

  // TODO, only queue so many packets if we haven't responded ever (to limit ignored unsupported channels)
  if(c->reliable)
  {
    channel3_miss_check(c,p);
    if(!channel3_seq_receive(c,p)) return; // queued, nothing to do
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
  channel3_queue(c);
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

typedef struct seq_struct
{
  uint32_t id, nextin, seen, acked;
  lob_t *in;
} *seq_t;

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

typedef struct miss_struct
{
  uint32_t nextack;
  lob_t *out;
} *miss_t;

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