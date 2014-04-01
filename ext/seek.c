#include "ext.h"


typedef struct seek_struct 
{
  hn_t id;
  int active;
  packet_t note;
} *seek_t;

typedef struct seeks_struct 
{
  xht_t active;
} *seeks_t;


seeks_t seeks_get(switch_t s)
{
  seeks_t sks;
  sks = xht_get(s->index,"seeks");
  if(sks) return sks;

  sks = malloc(sizeof (struct seeks_struct));
  memset(sks,0,sizeof (struct seeks_struct));
  sks->active = xht_new(11);
  xht_set(s->index,"seeks",sks);
  return sks;
}

seek_t seek_get(switch_t s, hn_t id)
{
  seek_t sk;
  seeks_t sks = seeks_get(s);
  sk = xht_get(sks->active,id->hexname);
  if(sk) return sk;

  sk = malloc(sizeof (struct seek_struct));
  memset(sk,0,sizeof (struct seek_struct));
  sk->id = id;
  xht_set(sks->active,id->hexname,sk);
  return sk;
}


void seek_handler(chan_t c)
{
  seek_t sk = (seek_t)c->arg;
  packet_t p = chan_pop(c);
  if(!sk || !p) return;
  DEBUG_PRINTF("seek response for %s of %.*s",sk->id->hexname,p->json_len,p->json);
  // process see array and end channel
  // sk->active-- and check to return note
}

void seek_send(switch_t s, seek_t sk, hn_t to)
{
  chan_t c;
  packet_t p;
  sk->active++;
  c = chan_new(s, to, "seek", 0);
  c->handler = seek_handler;
  c->arg = sk;
  p = chan_packet(c);
  packet_set_str(p,"seek",sk->id->hexname); // TODO make a prefix
  chan_send(c, p);
}

// create a seek to this hn and initiate connect
void _seek_auto(switch_t s, hn_t hn)
{
  seek_t sk = seek_get(s,hn);
  DEBUG_PRINTF("seek connecting %s",sk->id->hexname);
  // TODO get near from somewhere
  seek_send(s, sk, bucket_get(s->seeds, 0));
}

void seek_auto(switch_t s)
{
  s->handler = _seek_auto;
}

void seek_free(switch_t s)
{
  seeks_t sks = seeks_get(s);
  // TODO xht_walk active and free each one
  free(sks);
}

void seek_peer(switch_t s, hn_t to, hn_t id)
{
  // create peer channel
  // handler for response as raw packets w/ path
}


// just call back note instead of auto-connect
void seek_note(switch_t s, hn_t h, packet_t note)
{
  
}
