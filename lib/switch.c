#include "switch.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"

// a prime number for the internal hashtable used to track all active hashnames/lines
#define MAXPRIME 4211

switch_t switch_new(uint32_t prime)
{
  switch_t s;
  if(!(s = malloc(sizeof (struct switch_struct)))) return NULL;
  memset(s, 0, sizeof(struct switch_struct));
  s->cap = 256; // default cap size
  s->window = 32; // default reliable window size
  s->index = xht_new(prime?prime:MAXPRIME);
  s->parts = packet_new();
  if(!s->index || !s->parts) return switch_free(s);
  return s;
}

int switch_init(switch_t s, packet_t keys)
{
  int i = 0, err = 0;
  char *key, secret[10], csid, *pk, *sk;
  crypt_t c;

  while((key = packet_get_istr(keys,i)))
  {
    i += 2;
    if(strlen(key) != 2) continue;
    util_unhex((unsigned char*)key,2,(unsigned char*)&csid);
    sprintf(secret,"%s_secret",key);
    pk = packet_get_str(keys,key);
    sk = packet_get_str(keys,secret);
    DEBUG_PRINTF("loading pk %s sk %s",pk,sk);
    c = crypt_new(csid, (unsigned char*)pk, strlen(pk));
    if(crypt_private(c, (unsigned char*)sk, strlen(sk)))
    {
      err = 1;
      crypt_free(c);
      continue;
    }
    DEBUG_PRINTF("loaded %s",key);
    xht_set(s->index,(const char*)c->csidHex,(void *)c);
    packet_set_str(s->parts,c->csidHex,c->part);
  }
  
  packet_free(keys);
  if(err || !s->parts->json)
  {
    DEBUG_PRINTF("key loading failed");
    return 1;
  }
  s->id = hn_getparts(s->index, s->parts);
  if(!s->id) return 1;
  return 0;
}

switch_t switch_free(switch_t s)
{
  if(!s) return NULL;
  xht_free(s->index);
  packet_free(s->parts);
  if(s->seeds) bucket_free(s->seeds);
  free(s);
  return NULL;
}

void switch_capwin(switch_t s, int cap, int window)
{
  s->cap = cap;
  s->window = window;
}

// generate a broadcast/handshake ping packet
packet_t switch_ping(switch_t s)
{
  char *key, rand[8], trace[17];
  int i = 0;
  packet_t p = packet_new();
  packet_set_str(p,"type","ping");
  while((key = packet_get_istr(s->parts,i)))
  {
    i += 2;
    if(strlen(key) != 2) continue;
    packet_set(p,key,"true",4);
  }
  // get/gen token to validate pongs
  if(!(key = xht_get(s->index,"ping")))
  {
    xht_store(s->index,"ping",util_hex(crypt_rand((unsigned char*)rand,8),8,(unsigned char*)trace),17);
    key = trace;
  }
  packet_set_str(p,"trace",key);
  return p;
}

// caller must ensure it's ok to send a pong, and is responsible for ping and in
void switch_pong(switch_t s, packet_t ping, path_t in)
{
  char *key;
  unsigned char hex[3], csid = 0, best = 0;
  int i = 0;
  packet_t p;
  crypt_t c;

  // check our parts for the best match  
  while((key = packet_get_istr(s->parts,i)))
  {
    i += 2;
    if(strlen(key) != 2) continue;
    util_unhex((unsigned char*)key,2,(unsigned char*)&csid);
    if(csid <= best) continue;
    best = csid;
    memcpy(hex,key,3);
  }

  if(!best || !(c = xht_get(s->index,(char*)hex))) return;
  p = packet_new();
  packet_set_str(p,"type","pong");
  packet_set_str(p,"trace",packet_get_str(ping,"trace"));
  packet_set(p,"from",(char*)s->parts->json,s->parts->json_len);
  packet_body(p,c->key,c->keylen);
  p->out = path_copy(in);
  switch_sendingQ(s,p);
}

void switch_loop(switch_t s)
{
  // give all channels a chance
}

void switch_seed(switch_t s, hn_t hn)
{
  if(!s->seeds) s->seeds = bucket_new();
  bucket_add(s->seeds, hn);
}

packet_t switch_sending(switch_t s)
{
  packet_t p;
  if(!s || !s->out) return NULL;
  p = s->out;
  s->out = p->next;
  if(!s->out) s->last = NULL;
  return p;
}

// internally adds to sending queue
void switch_sendingQ(switch_t s, packet_t p)
{
  packet_t dup;
  if(!p) return;

  // if there's no path, find one or copy to many
  if(!p->out)
  {
    // just being paranoid
    if(!p->to)
    {
      packet_free(p);
      return;
    }

    // if the last path is alive, just use that
    if(path_alive(p->to->last)) p->out = p->to->last;
    else{
      int i;
      // try sending to all paths
      for(i=0; p->to->paths[i]; i++)
      {
        dup = packet_copy(p);
        dup->out = p->to->paths[i];
        switch_sendingQ(s, dup);
      }
      packet_free(p);
      return;   
    }
  }

  // update stats
  p->out->atOut = platform_seconds();

  // add to the end of the queue
  if(s->last)
  {
    s->last->next = p;
    s->last = p;
    return;
  }
  s->last = s->out = p;  
}

// tries to send an open if we haven't
void switch_open(switch_t s, hn_t to, path_t direct)
{
  packet_t open, inner;

  if(!to) return;
  if(!to->c)
  {
    DEBUG_PRINTF("can't open, no key for %s",to->hexname);
    if(s->handler) s->handler(s,to);
    return;
  }

  // actually send the open
  inner = packet_new();
  packet_set_str(inner,"to",to->hexname);
  packet_set(inner,"from",(char*)s->parts->json,s->parts->json_len);
  open = crypt_openize((crypt_t)xht_get(s->index,to->hexid), to->c, inner);
  DEBUG_PRINTF("opening to %s %hu %s",to->hexid,packet_len(open),to->hexname);
  if(!open) return;
  open->to = to;
  if(direct) open->out = direct;
  switch_sendingQ(s, open);
}

void switch_send(switch_t s, packet_t p)
{
  packet_t lined;

  if(!p) return;
  
  // require recipient at least, and not us
  if(!p->to || p->to == s->id) return (void)packet_free(p);

  // encrypt the packet to the line, chains together
  lined = crypt_lineize(p->to->c, p);
  if(lined) return switch_sendingQ(s, lined);

  // queue most recent packet to be sent after opened
  if(p->to->onopen) packet_free(p->to->onopen);
  p->to->onopen = p;

  // no line, so generate open instead
  switch_open(s, p->to, NULL);
}

chan_t switch_pop(switch_t s)
{
  chan_t c;
  if(!s->chans) return NULL;
  c = s->chans;
  chan_dequeue(c);
  return c;
}

void switch_receive(switch_t s, packet_t p, path_t in)
{
  hn_t from;
  packet_t inner;
  crypt_t c;
  chan_t chan;
  char hex[3];
  char lineHex[33];

  if(!s || !p || !in) return;

  // handle open packets
  if(p->json_len == 1)
  {
    util_hex(p->json,1,(unsigned char*)hex);
    c = xht_get(s->index,hex);
    if(!c) return (void)packet_free(p);
    inner = crypt_deopenize(c, p);
    DEBUG_PRINTF("DEOPEN %d",inner);
    if(!inner) return (void)packet_free(p);

    from = hn_frompacket(s->index, inner);
    if(crypt_line(from->c, inner) != 0) return; // not new/valid, ignore
    
    // line is open!
    DEBUG_PRINTF("line in %d %s %d %s",from->c->lined,from->hexname,from,from->c->lineHex);
    if(from->c->lined == 1) chan_reset(s, from);
    xht_set(s->index, (const char*)from->c->lineHex, (void*)from);
    in = hn_path(from, in);
    switch_open(s, from, in); // in case we need to send an open
    if(from->onopen)
    {
      packet_t last = from->onopen;
      from->onopen = NULL;
      last->out = in;
      switch_send(s, last);
    }
    return;
  }

  // handle line packets
  if(p->json_len == 0)
  {
    util_hex(p->body, 16, (unsigned char*)lineHex);
    from = xht_get(s->index, lineHex);
    if(from)
    {
      in = hn_path(from, in);
      p = crypt_delineize(from->c, p);
      if(!p)
      {
        DEBUG_PRINTF("invlaid line from %s %s",path_json(in),from->hexname);
        return;
      }
      
      // route to the channel
      if((chan = chan_in(s, from, p)))
      {
        // if new channel w/ seq, configure as reliable
        if(chan->state == CHAN_STARTING && packet_get_str(p,"seq")) chan_reliable(chan, s->window);
        return chan_receive(chan, p);
      }

      // bounce it!
      if(!packet_get_str(p,"err"))
      {
        packet_set_str(p,"err","unknown channel");
        p->to = from;
        p->out = in;
        switch_send(s, p);          
      }else{
        packet_free(p);
      }
      return;
    }
  }

  // handle valid pong responses, start handshake
  if(util_cmp("pong",packet_get_str(p,"type")) == 0 && util_cmp(xht_get(s->index,"ping"),packet_get_str(p,"trace")) == 0 && (from = hn_fromjson(s->index,p)) != NULL)
  {
    DEBUG_PRINTF("pong from %s",from->hexname);
    in = hn_path(from, in);
    switch_open(s,from,in);
    packet_free(p);
    return;
  }

  // handle pings, respond if local only or dedicated seed
  if(util_cmp("ping",packet_get_str(p,"type")) == 0 && (s->isSeed || path_local(in)))
  {
    switch_pong(s,p,in);
    packet_free(p);
    return;
  }

  // nothing processed, clean up
  packet_free(p);
}

// sends a note packet to it's channel if it can, !0 for error
int switch_note(switch_t s, packet_t note)
{
  chan_t c;
  packet_t notes;
  if(!s || !note) return -1;
  c = xht_get(s->index,packet_get_str(note,".to"));
  if(!c) return -1;
  notes = c->notes;
  while(notes) notes = notes->next;
  if(!notes) c->notes = note;
  else notes->next = note;
  chan_queue(c);
  return 0;

}
