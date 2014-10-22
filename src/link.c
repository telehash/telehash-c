#include "link.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mesh.h"

// internal structure to manage our link-local state about pipes

typedef struct seen_struct
{
  pipe_t pipe;
  uint32_t at;
  struct seen_struct *next;
} *seen_t;


link_t link_new(mesh_t mesh, hashname_t id)
{
  link_t link;

  if(!mesh || !id) return LOG("invalid args");

  LOG("adding link %s",id->hashname);
  if(!(link = malloc(sizeof (struct link_struct)))) return (link_t)hashname_free(id);
  memset(link,0,sizeof (struct link_struct));
  
  link->id = id;
  link->mesh = mesh;
  xht_set(mesh->index,id->hashname,link);

  return link;
}

void link_free(link_t link)
{
  if(!link) return;
  LOG("dropping link %s",link->id->hashname);
  xht_set(link->mesh->index,link->id->hashname,NULL);

  // TODO go through ->pipes

  hashname_free(link->id);
  free(link);
}

link_t link_get(mesh_t mesh, char *hashname)
{
  link_t link;
  hashname_t id;

  if(!mesh || !hashname) return LOG("invalid args");
  link = xht_get(mesh->index,hashname);
  if(!link)
  {
    id = hashname_str(hashname);
    if(!id) return LOG("invalid hashname %s",hashname);
    link = link_new(mesh,id);
  }

  return link;
}

link_t link_keys(mesh_t mesh, lob_t keys)
{
  uint8_t csid;

  if(!mesh || !keys) return LOG("invalid args");
  csid = hashname_id(mesh->keys,keys);
  if(!csid) return LOG("no supported key");
  return link_key(mesh, hashname_im(keys,csid));
}

link_t link_key(mesh_t mesh, lob_t key)
{
  uint8_t csid;
  hashname_t hn;
  link_t link;

  if(!mesh || !key) return LOG("invalid args");
  csid = hashname_id(mesh->keys,key);
  if(!csid) return LOG("no supported key");

  hn = hashname_key(key);
  if(!hn) return LOG("invalid key");

  link = link_get(mesh, hn->hashname);
  if(link)
  {
    hashname_free(hn);
  }else{
    link = link_new(mesh,hn);
  }
  
  // key is new, add exchange
  if(!link->key)
  {
    LOG("adding %x key to link %s",csid,link->id->hashname);
    link->csid = csid;
    link->key = lob_copy(key);
    link->x = exchange3_new(link->mesh->self, csid, key, platform_seconds());
  }

  return link;
}

// try to turn a path into a pipe
pipe_t link_path(link_t link, lob_t path)
{
  pipe_t pipe;
  uint8_t i;
  seen_t seen;

  if(!link || !path) return LOG("bad args");

  // check all the transports
  for(i=0; link->mesh->net[i] && i < MAXNET; i++)
  {
    if(!(pipe = link->mesh->net[i](link,path))) continue;
    // see if we've seen it already
    for(seen = link->pipes; seen; seen = seen->next)
    {
      if(seen->pipe == pipe) return pipe;
    }
    if(!(seen = malloc(sizeof (struct seen_struct)))) return NULL;
    memset(seen,0,sizeof (struct seen_struct));
    seen->pipe = pipe;
    seen->next = link->pipes;
    link->pipes = seen;
    return pipe;
  }
  
  return LOG("path not supported %.*s",path->head_len,path->head);
}

// process an incoming handshake
uint8_t link_handshake(link_t link, lob_t inner, lob_t outer, pipe_t pipe)
{
  return 11;
}

// process a decrypted channel packet
uint8_t link_receive(link_t link, lob_t inner, pipe_t pipe)
{
  return 11;
}

/*
// flags channel as ended either in or out
void doend(link_t c)
{
  if(!c || c->ended) return;
  DEBUG_PRINTF("channel ended %d",c->id);
  c->ended = 1;
}

// immediately removes channel, creates/sends packet to app to notify
void doerror(link_t c, lob_t p, char *err)
{
  if(c->ended) return;
  DEBUG_PRINTF("channel fail %d %s %d %d %d",c->id,err?err:packet_get_str(p,"err"),c->timeout,c->tsent,c->trecv);

  // unregister for any more packets immediately
  xht_set(c->to->chans,(char*)c->hexid,NULL);
  if(c->id > c->to->chanMax) c->to->chanMax = c->id;

  // convenience to generate error packet
  if(!p)
  {
    if(!(p = packet_new())) return;
    packet_set_str(p,"err",err?err:"unknown");
    packet_set_int(p,"c",c->id);
  }

  // send error to app immediately, will doend() when processed
  p->next = c->in;
  c->in = p;
  link_queue(c);
}

void link_free(link_t c)
{
  lob_t p;
  if(!c) return;

  // if there's an arg set, we can't free and must notify channel handler
  if(c->arg) return link_queue(c);

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
  link_dequeue(c);
  if(c->reliable)
  {
    link_seq_free(c);
    link_miss_free(c);
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
  // TODO, if it's the last channel, bucket_rem(c->s->active, c);
  free(c->type);
  free(c);
}

void walkreset(xht_t h, const char *key, void *val, void *arg)
{
  link_t c = (link_t)val;
  if(!c) return;
  if(c->opened) return doerror(c,NULL,"reset");
  // flush any waiting packets
  if(c->reliable) link_miss_resend(c);
  else switch_send(c->s,packet_copy(c->in));
}
void link_reset(switch_t s, hn_t to)
{
  // fail any existing open channels
  xht_walk(to->chans, &walkreset, NULL);
  // reset max id tracking
  to->chanMax = 0;
}

void walktick(xht_t h, const char *key, void *val, void *arg)
{
  uint32_t last;
  link_t c = (link_t)val;

  // latent cleanup/free
  if(c->ended && (c->tfree++) > c->timeout) return link_free(c);

  // any channel can get tick event
  if(c->tick) c->tick(c);

  // these are administrative/internal channels (to our own hashname)
  if(c->to == c->s->id) return;

  // do any auto-resend packet timers
  if(c->tresend)
  {
    // TODO check packet resend timers, link_miss_resend or c->in
  }

  // unreliable timeout, use last received unless none, then last sent
  if(!c->reliable)
  {
    last = (c->trecv) ? c->trecv : c->tsent;
    if(c->timeout && (c->s->tick - last) > c->timeout) doerror(c,NULL,"timeout");
  }

}

void link_tick(switch_t s, hn_t hn)
{
  xht_walk(hn->chans, &walktick, NULL);
}

link_t link_reliable(link_t c, int window)
{
  if(!c || !window || c->tsent || c->trecv || c->reliable) return c;
  c->reliable = window;
  link_seq_init(c);
  link_miss_init(c);
  return c;
}

// kind of a macro, just make a reliable channel of this type
link_t link_start(switch_t s, char *hn, char *type)
{
  link_t c;
  if(!s || !hn) return NULL;
  c = link_new(s, hn_gethex(s->index,hn), type, 0);
  return link_reliable(c, s->window);
}

link_t link_new(switch_t s, hn_t to, char *type, uint32_t id)
{
  link_t c;
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
  c = malloc(sizeof (struct link_struct));
  memset(c,0,sizeof (struct link_struct));
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

void link_timeout(link_t c, uint32_t seconds)
{
  if(!c) return;
  c->timeout = seconds;
}

link_t link_in(switch_t s, hn_t from, lob_t p)
{
  link_t c;
  unsigned long id;
  char hexid[9];
  if(!from || !p) return NULL;

  id = strtol(packet_get_str(p,"c"), NULL, 10);
  util_hex((unsigned char*)&id,4,(unsigned char*)hexid);
  c = xht_get(from->chans, hexid);
  if(c) return c;

  return link_new(s, from, packet_get_str(p, "type"), id);
}

// get the next incoming note waiting to be handled
lob_t link_notes(link_t c)
{
  lob_t note;
  if(!c) return NULL;
  note = c->notes;
  if(note) c->notes = note->next;
  return note;
}

// create a new note tied to this channel
lob_t link_note(link_t c, lob_t note)
{
  if(!note) note = packet_new();
  packet_set_str(note,".from",(char*)c->uid);
  return note;
}

// send this note back to the sender
int link_reply(link_t c, lob_t note)
{
  char *from;
  if(!c || !(from = packet_get_str(note,".from"))) return -1;
  packet_set_str(note,".to",from);
  packet_set_str(note,".from",(char*)c->uid);
  return switch_note(c->s,note);
}

// create a packet ready to be sent for this channel
lob_t link_packet(link_t c)
{
  lob_t p;
  if(!c) return NULL;
  p = c->reliable?link_seq_packet(c):packet_new();
  if(!p) return NULL;
  p->to = c->to;
  if(!c->tsent && !c->trecv)
  {
    packet_set_str(p,"type",c->type);
  }
  packet_set_int(p,"c",c->id);
  return p;
}

lob_t link_pop(link_t c)
{
  lob_t p;
  if(!c) return NULL;
  // this allows errors to be processed immediately
  if((p = c->in))
  {
    c->in = p->next;
    if(!c->in) c->inend = NULL;    
  }
  if(!p && c->reliable) p = link_seq_pop(c);

  // change state as soon as an err/end is processed
  if(packet_get_str(p,"err") || util_cmp(packet_get_str(p,"end"),"true") == 0) doend(c);
  return p;
}

// add to processing queue
void link_queue(link_t c)
{
  link_t step;
  // add to switch queue
  step = c->s->chans;
  if(c->next || step == c) return;
  while(step && (step = step->next)) if(step == c) return;
  c->next = c->s->chans;
  c->s->chans = c;
}

// remove from processing queue
void link_dequeue(link_t c)
{
  link_t step = c->s->chans;
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
void link_receive(link_t c, lob_t p)
{
  if(!c || !p) return;
  DEBUG_PRINTF("channel in %d %d %.*s",c->id,p->body_len,p->json_len,p->json);
  c->trecv = c->s->tick;
  DEBUG_PRINTF("recv %d",c);
  
  // errors are immediate
  if(packet_get_str(p,"err")) return doerror(c,p,NULL);

  if(!c->opened)
  {
    DEBUG_PRINTF("channel opened %d",c->id);
    // remove any cached outgoing packet
    packet_free(c->in);
    c->in = NULL;
    c->opened = 1;
  }

  // TODO, only queue so many packets if we haven't responded ever (to limit ignored unsupported channels)
  if(c->reliable)
  {
    link_miss_check(c,p);
    if(!link_seq_receive(c,p)) return; // queued, nothing to do
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
  link_queue(c);
}

// convenience
void link_end(link_t c, lob_t p)
{
  if(!c) return;
  if(!p && !(p = link_packet(c))) return;
  packet_set(p,"end","true",4);
  link_send(c,p);
}

// send errors directly
link_t link_fail(link_t c, char *err)
{
  lob_t p;
  if(!c) return NULL;
  if(!(p = packet_new())) return c;
  doend(c);
  p->to = c->to;
  packet_set_str(p,"err",err?err:"unknown");
  packet_set_int(p,"c",c->id);
  switch_send(c->s,p);
  return c;
}

// smartly send based on what type of channel we are
void link_send(link_t c, lob_t p)
{
  if(!p) return;
  if(!c) return (void)packet_free(p);
  DEBUG_PRINTF("channel out %d %.*s",c->id,p->json_len,p->json);
  c->tsent = c->s->tick;
  if(c->reliable && packet_get_str(p,"seq")) p = packet_copy(p); // miss tracks the original p = link_packet(), a sad hack
  if(packet_get_str(p,"err") || util_cmp(packet_get_str(p,"end"),"true") == 0) doend(c); // track sending error/end
  else if(!c->reliable && !c->opened && !c->in) c->in = packet_copy(p); // if normal unreliable start packet, track for resends
  
  switch_send(c->s,p);
}

// optionally sends reliable channel ack-only if needed
void link_ack(link_t c)
{
  lob_t p;
  if(!c || !c->reliable) return;
  p = link_seq_ack(c,NULL);
  if(!p) return;
  DEBUG_PRINTF("channel ack %d %.*s",c->id,p->json_len,p->json);
  switch_send(c->s,p);
}

*/