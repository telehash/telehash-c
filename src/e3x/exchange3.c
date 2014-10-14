#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "exchange3.h"
#include "platform.h"

// make a new exchange
// packet must contain the raw key in the body
exchange3_t exchange3_new(self3_t self, uint8_t csid, lob_t key, uint32_t at)
{
  uint8_t i, token[16];
  exchange3_t x;
  remote_t remote;
  cipher3_t cs = NULL;

  if(!self || !csid || !key || !key->body_len) return LOG("bad args");

  // find matching csid
  for(i=0; i<CS_MAX; i++)
  {
    if(cipher3_sets[i]->csid != csid) continue;
    if(!self->locals[i]) continue;
    cs = cipher3_sets[i];
    break;
  }

  if(!cs) return LOG("unsupported csid %x",csid);
  remote = cs->remote_new(key, token);
  if(!remote) return LOG("failed to create %x remote %s",csid,cs->err());

  if(!(x = malloc(sizeof (struct exchange3_struct)))) return NULL;
  memset(x,0,sizeof (struct exchange3_struct));

  x->csid = csid;
  x->remote = remote;
  x->cs = cs;
  x->self = self;
  memcpy(x->token,token,16);
  
  // determine order, if we sort first, we're even
  for(i = 0; i < key->body_len; i++)
  {
    if(key->body[i] == self->keys[cs->id]->body[i]) continue;
    x->order = (key->body[i] > self->keys[cs->id]->body[i]) ? 2 : 1;
  }
  x->cid = x->order;

  // make sure at matches order
  x->at = at;
  if(x->order == 2)
  {
    if(at % 2 != 0) x->at++;
  }else{
    if(at % 2 == 0) x->at++;
  }
  
  return x;
}

void exchange3_free(exchange3_t x)
{
  if(!x) return;
  x->cs->remote_free(x->remote);
  free(x);
}

// these require a self (local) and an exchange (remote) but are exchange independent
// will safely set/increment at if 0
lob_t exchange3_message(exchange3_t x, lob_t inner)
{
  if(!x || !inner) return LOG("bad args");
  return x->cs->remote_encrypt(x->remote,x->self->locals[x->cs->id],inner);
}

// any handshake verify fail (lower seq), always resend handshake
uint8_t exchange3_verify(exchange3_t x, lob_t outer)
{
  if(!x || !outer) return 1;
  return x->cs->remote_verify(x->remote,x->self->locals[x->cs->id],outer);
}

// returns the seq value for a handshake reply if needed
// sets secrets/seq/cids to the given handshake if it's newer
// always call chan_sync(c,true) after this on all open channels to signal them the exchange is active
uint32_t exchange3_sync(exchange3_t x, lob_t handshake)
{
  return 0;
}

// just a convenience, seq=0 means force new handshake (and call chan_sync(false)), or seq = exchange3_seq() or exchange3_sync()
lob_t exchange3_handshake(exchange3_t x, lob_t inner, uint32_t at)
{
  return NULL;
}

// simple encrypt/decrypt conversion of any packet for channels
lob_t exchange3_receive(exchange3_t x, lob_t outer)
{
  return NULL;
}

// comes from channel 
lob_t exchange3_send(exchange3_t x, lob_t inner)
{
  return NULL;
}

// get next avail outgoing channel id
uint32_t exchange3_cid(exchange3_t x)
{
  if(!x) return 0;
  return x->cid++;
}

