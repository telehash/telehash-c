#include "telehash.h"

/*
typedef struct seek_struct 
{
  hashname_t id;
  int active;
  lob_t note;
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

seek_t seek_get(switch_t s, hashname_t id)
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

void peer_handler(chan_t c)
{
  // remove the nat punch path if any
  if(c->arg)
  {
    path_free((path_t)c->arg);
    c->arg = NULL;
  }

  DEBUG_PRINTF("peer handler %s",c->to->hexname);
  // TODO process relay'd packets
}

// csid may be address format
void peer_send(switch_t s, hashname_t to, char *address)
{
  char *csid, *ip = NULL, *port;
  lob_t punch = NULL;
  crypt_t cs;
  chan_t c;
  lob_t p;

  if(!address) return;
  if(!(csid = strchr(address,','))) return;
  *csid = 0;
  csid++;
  // optional address ,ip,port for punch
  if((ip = strchr(csid,',')))
  {
    *ip = 0;
    ip++;
  }
  if(!(cs = xht_get(s->index,csid))) return;

  // new peer channel
  c = chan_new(s, to, "peer", 0);
  c->handler = peer_handler;
  p = chan_packet(c);
  lob_set_str(p,"peer",address);
  lob_body(p,cs->key,cs->keylen);

  // send the nat punch packet if ip,port is given
  if(ip && (port = strchr(ip,',')))
  {
    *port = 0;
    port++;
    punch = lob_new();
    c->arg = punch->out = path_new("ipv4"); // free path w/ peer channel cleanup
    path_ip(punch->out,ip);
    path_port(punch->out,atoi(port));
    switch_sendingQ(s,punch);
  }

  chan_send(c, p);
}

void seek_handler(chan_t c)
{
  int i = 0;
  char *address;
  seek_t sk = (seek_t)c->arg;
  lob_t see, p = chan_pop(c);
  if(!sk || !p) return;
  DEBUG_PRINTF("seek response for %s of %.*s",sk->id->hexname,p->json_len,p->json);

  // process see array and end channel
  see = lob_get_packet(p,"see");
  while((address = lob_get_istr(see,i)))
  {
    i++;
    if(strncmp(address,sk->id->hexname,64) == 0) peer_send(c->s, c->to, address);
    // TODO maybe recurse others
  }
  lob_free(see);
  lob_free(p);
  // TODO sk->active-- and check to return note
}

void seek_send(switch_t s, seek_t sk, hashname_t to)
{
  chan_t c;
  lob_t p;
  sk->active++;
  c = chan_new(s, to, "seek", 0);
  c->handler = seek_handler;
  c->arg = sk;
  p = chan_packet(c);
  lob_set_str(p,"seek",sk->id->hexname); // TODO make a prefix
  chan_send(c, p);
}

// create a seek to this hn and initiate connect
void _seek_auto(switch_t s, hashname_t hn)
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

// just call back note instead of auto-connect
void seek_note(switch_t s, hashname_t h, lob_t note)
{
  
}
*/