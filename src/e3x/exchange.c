#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "telehash.h"

// make a new exchange
// packet must contain the raw key in the body
e3x_exchange_t e3x_exchange_new(e3x_self_t self, uint8_t csid, lob_t key)
{
  uint8_t i, token[16];
  e3x_exchange_t x;
  remote_t remote;
  e3x_cipher_t cs = NULL;

  if(!self || !csid || !key || !key->body_len) return LOG("bad args");

  // find matching csid
  for(i=0; i<CS_MAX; i++)
  {
    if(!e3x_cipher_sets[i]) continue;
    if(e3x_cipher_sets[i]->csid != csid) continue;
    if(!self->locals[i]) continue;
    cs = e3x_cipher_sets[i];
    break;
  }

  if(!cs) return LOG("unsupported csid %x",csid);
  remote = cs->remote_new(key, token);
  if(!remote) return LOG("failed to create %x remote %s",csid,cs->err());

  if(!(x = malloc(sizeof (struct e3x_exchange_struct)))) return NULL;
  memset(x,0,sizeof (struct e3x_exchange_struct));

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
    break;
  }
  x->cid = x->order;

  return x;
}

void e3x_exchange_free(e3x_exchange_t x)
{
  if(!x) return;
  x->cs->remote_free(x->remote);
  x->cs->ephemeral_free(x->ephem);
  free(x);
}

// these require a self (local) and an exchange (remote) but are exchange independent
// will safely set/increment at if 0
lob_t e3x_exchange_message(e3x_exchange_t x, lob_t inner)
{
  if(!x || !inner) return LOG("bad args");
  return x->cs->remote_encrypt(x->remote,x->self->locals[x->cs->id],inner);
}

// any handshake verify fail (lower seq), always resend handshake
uint8_t e3x_exchange_verify(e3x_exchange_t x, lob_t outer)
{
  if(!x || !outer) return 1;
  return x->cs->remote_verify(x->remote,x->self->locals[x->cs->id],outer);
}

uint8_t e3x_exchange_validate(e3x_exchange_t x, lob_t args, lob_t sig, uint8_t *data, size_t len)
{
  remote_t remote = NULL;
  e3x_cipher_t cs = NULL;
  char *alg = lob_get(args,"alg");
  if(!data || !len || !alg) return 1;
  cs = e3x_cipher_set(0,alg);
  if(!cs || !cs->remote_validate)
  {
    LOG("no validate support for %s",alg);
    return 1;
  }
  if(x && x->cs == cs) remote = x->remote;
  return cs->remote_validate(remote,args,sig,data,len);
}

// will return the current outgoing at value, optional arg to update it
uint32_t e3x_exchange_out(e3x_exchange_t x, uint32_t at)
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
uint32_t e3x_exchange_in(e3x_exchange_t x, uint32_t at)
{
  if(!x) return 0;

  // ensure at is newer and valid, or acking our out
  if(at && at > x->in && at >= x->out && (((at % 2)+1) == x->order || at == x->out)) x->in = at;

  return x->in;
}

// drops ephemeral state, out=0
e3x_exchange_t e3x_exchange_down(e3x_exchange_t x)
{
  if(!x) return NULL;
  x->out = 0;
  if(x->ephem)
  {
    x->cs->ephemeral_free(x->ephem);
    x->ephem = NULL;
    memset(x->eid,0,16);
    x->last = 0;
  }
  return x;
}

// synchronize to incoming ephemeral key and set out at = in at, returns x if success, NULL if not
e3x_exchange_t e3x_exchange_sync(e3x_exchange_t x, lob_t outer)
{
  ephemeral_t ephem;
  if(!x || !outer) return LOG("bad args");
  if(outer->body_len < 16) return LOG("outer too small");

  if(x->in > x->out) x->out = x->in;

  // if the incoming ephemeral key is different, create a new ephemeral
  if(util_ct_memcmp(outer->body,x->eid,16) != 0)
  {
    ephem = x->cs->ephemeral_new(x->remote,outer);
    if(!ephem) return LOG("ephemeral creation failed %s",x->cs->err());
    x->cs->ephemeral_free(x->ephem);
    x->ephem = ephem;
    memcpy(x->eid,outer->body,16);
    // reset incoming channel id validation
    x->last = 0;
  }

  return x;
}

// just a convenience, generates handshake w/ current e3x_exchange_at value
lob_t e3x_exchange_handshake(e3x_exchange_t x, lob_t inner)
{
  lob_t tmp;
  uint8_t i;
  uint8_t local = 0;
  if(!x) return LOG("invalid args");
  if(!x->out) return LOG("no out set");

  // create deprecated key handshake inner from all supported csets
  if(!inner)
  {
    local = 1;
    inner = lob_new();
    lob_set(inner, "type", "key");
    // loop through all ciphersets for any keys
    for(i=0; i<CS_MAX; i++)
    {
      if(!(tmp = x->self->keys[i])) continue;
      // this csid's key is the body, rest is intermediate in json
      if(e3x_cipher_sets[i] == x->cs)
      {
        lob_body(inner,tmp->body,tmp->body_len);
      }else{
        uint8_t hash[32];
        e3x_hash(tmp->body,tmp->body_len,hash);
        lob_set_base32(inner,e3x_cipher_sets[i]->hex,hash,32);
      }
    }
  }

  // set standard values
  lob_set_uint(inner,"at",x->out);

  tmp = e3x_exchange_message(x, inner);
  if(!local) return tmp;
  return lob_link(tmp, inner);
}

// simple encrypt/decrypt conversion of any packet for channels
lob_t e3x_exchange_receive(e3x_exchange_t x, lob_t outer)
{
  lob_t inner;
  if(!x || !outer) return LOG("invalid args");
  if(!x->ephem) return LOG("no handshake");
  inner = x->cs->ephemeral_decrypt(x->ephem,outer);
  if(!inner) return LOG("decryption failed %s",x->cs->err());
  LOG("decrypted head %d body %d",inner->head_len,inner->body_len);
  return inner;
}

// comes from channel
lob_t e3x_exchange_send(e3x_exchange_t x, lob_t inner)
{
  lob_t outer;
  if(!x || !inner) return LOG("invalid args");
  if(!x->ephem) return LOG("no handshake");
  LOG("encrypting head %d body %d",inner->head_len,inner->body_len);
  outer = x->cs->ephemeral_encrypt(x->ephem,inner);
  if(!outer) return LOG("encryption failed %s",x->cs->err());
  return outer;
}

// validate the next incoming channel id from the packet, or return the next avail outgoing channel id
uint32_t e3x_exchange_cid(e3x_exchange_t x, lob_t incoming)
{
  uint32_t cid;
  if(!x) return 0;

  // in outgoing mode, just return next valid one
  if(!incoming)
  {
    cid = x->cid;
    x->cid += 2;
    return cid;
  }

  // incoming mode, verify it
  if(!(cid = lob_get_uint(incoming,"c"))) return 0;
  if(cid <= x->last) return 0; // can't re-use old ones
  // make sure it's even/odd properly
  if((cid % 2) == (x->cid % 2)) return 0;
  x->last = cid; // track the highest
  return cid;
}

uint8_t *e3x_exchange_token(e3x_exchange_t x)
{
  if(!x) return NULL;
  return x->token;
}
