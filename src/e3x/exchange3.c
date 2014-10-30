#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "exchange3.h"
#include "platform.h"

// make a new exchange
// packet must contain the raw key in the body
exchange3_t exchange3_new(self3_t self, uint8_t csid, lob_t key)
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

// will return the current outgoing at value, optional arg to update it
uint32_t exchange3_out(exchange3_t x, uint32_t at)
{
  if(!x) return 0;

  // if there's a base, update at
  if(at && at > x->out)
  {
    // make sure at matches order
    x->out = at;
    if(x->order == 2)
    {
      if(x->out % 2 != 0) x->out++;
    }else{
      if(x->out % 2 == 0) x->out++;
    }
  }
  
  return x->out;
}

// return the current incoming at value, optional arg to update it
uint32_t exchange3_in(exchange3_t x, uint32_t at)
{
  if(!x) return 0;

  // ensure at is newer and valid
  if(at && at > x->in && ((at % 2)+1) == x->order) x->in = at;
  
  return x->in;
}

// synchronize to incoming ephemeral key and set out at = in at, returns x if success, NULL if not
exchange3_t exchange3_sync(exchange3_t x, lob_t outer)
{
  ephemeral_t ephem;
  if(!x || !outer) return LOG("bad args");
  if(outer->body_len < 16) return LOG("outer too small");
  
  if(x->in > x->out) x->out = x->in;

  // if the incoming ephemeral key is different, create a new ephemeral
  if(memcmp(outer->body,x->eid,16) != 0)
  {
    ephem = x->cs->ephemeral_new(x->remote,outer);
    if(!ephem) return LOG("ephemeral creation failed %s",x->cs->err());
    x->cs->ephemeral_free(x->ephem);
    x->ephem = ephem;
    memcpy(x->eid,outer->body,16);
  }

  return x;
}

// just a convenience, generates handshake w/ current exchange3_at value
lob_t exchange3_handshake(exchange3_t x)
{
  lob_t inner, key;
  uint8_t i;
  if(!x) return LOG("invalid args");
  if(!x->out) return LOG("no out set");

  // create new handshake inner from all supported csets
  inner = lob_new();
  lob_set_int(inner,"at",x->out);
  
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

uint8_t *exchange3_token(exchange3_t x)
{
  if(!x) return NULL;
  return x->token;
}

