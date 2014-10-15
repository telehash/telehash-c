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
  x->cs->ephemeral_free(x->ephem);
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
// always call chan_sync(c,true||false) after this on all open channels to signal them
uint32_t exchange3_sync(exchange3_t x, lob_t outer, lob_t inner)
{
  uint32_t at;
  ephemeral_t ephem;
  if(!x || !outer || !inner) return x->at;
  if(outer->body_len < 16) return x->at;
  
  // if the inner at is older than ours, send ours
  at = lob_get_int(inner,"at");
  if(at < x->at) return x->at;

  // if the incoming handshake's key is different, create a new ephemeral
  if(memcmp(outer->body,x->etoken,16) != 0)
  {
    ephem = x->cs->ephemeral_new(x->remote,outer);
    if(!ephem)
    {
      LOG("ephemeral creation failed %s",x->cs->err());
      return 0;
    }
    x->cs->ephemeral_free(x->ephem);
    x->ephem = ephem;
  }

  return at;
}

// just a convenience, at=0 means force new handshake (and chan_sync(false)), or pass at from exchange3_sync()
lob_t exchange3_handshake(exchange3_t x, uint32_t at)
{
  lob_t inner, key;
  uint8_t i;
  if(!x) return LOG("invalid args");

  // no at, force us out of sync
  if(!at)
  {
    x->at += 2;
    at = x->at;
  }

  // create new handshake inner from all supported csets
  inner = lob_new();
  lob_set_int(inner,"at",at);
  
  // loop through all ciphersets for any keys
  for(i=0; i<CS_MAX; i++)
  {
    if(!(key = x->self->keys[i])) continue;
    // this csid's key is the body, rest is intermediate in json
    if(cipher3_sets[i] == x->cs)
    {
      lob_body(inner,key->body,key->body_len);
    }else{
      lob_set(inner,cipher3_sets[i]->hex,lob_get(key,"hash"));
    }
  }

  return exchange3_message(x, inner);
}

// simple encrypt/decrypt conversion of any packet for channels
lob_t exchange3_receive(exchange3_t x, lob_t outer)
{
  lob_t inner;
  if(!x || !outer) return LOG("invalid args");
  if(!x->ephem) return LOG("no handshake");
  inner = x->cs->ephemeral_decrypt(x->ephem,outer);
  if(!inner) return LOG("decryption failed %s",x->cs->err());
  // TODO validate channel packet / cid
  return inner;
}

// comes from channel 
lob_t exchange3_send(exchange3_t x, lob_t inner)
{
  lob_t outer;
  if(!x || !inner) return LOG("invalid args");
  if(!x->ephem) return LOG("no handshake");
  outer = x->cs->ephemeral_encrypt(x->ephem,inner);
  if(!outer) return LOG("encryption failed %s",x->cs->err());
  return outer;
}

// get next avail outgoing channel id
uint32_t exchange3_cid(exchange3_t x)
{
  if(!x) return 0;
  return x->cid++;
}

