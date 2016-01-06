#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "telehash.h"

// load secrets/keys to create a new local endpoint
e3x_self_t e3x_self_new(lob_t secrets, lob_t keys)
{
  uint8_t i, csids = 0;
  e3x_self_t self;
  if(!keys) keys = lob_linked(secrets); // convenience
  if(!keys) return NULL;

  if(!(self = malloc(sizeof (struct e3x_self_struct)))) return NULL;
  memset(self,0,sizeof (struct e3x_self_struct));

  // give each cset a chance to make a local
  for(i=0; i<CS_MAX; i++)
  {
    if(!e3x_cipher_sets[i] || !e3x_cipher_sets[i]->local_new) continue;
    self->locals[i] = e3x_cipher_sets[i]->local_new(keys, secrets);
    if(!self->locals[i]) continue;
    // make a copy of the binary keys for comparison logic
    self->keys[i] = lob_get_base32(keys, e3x_cipher_sets[i]->hex);
    csids++;
  }

  if(!csids)
  {
    e3x_self_free(self);
    return LOG("self failed for %.*s",keys->head_len,keys->head);
  }

  LOG("self created with %d csids",csids);
  return self;
}

// any exchanges must have been free'd first
void e3x_self_free(e3x_self_t self)
{
  uint8_t i;
  if(!self) return;

  // free any locals created
  for(i=0; i<CS_MAX; i++)
  {
    if(!self->locals[i] || !e3x_cipher_sets[i]) continue;
    e3x_cipher_sets[i]->local_free(self->locals[i]);
    lob_free(self->keys[i]);
  }

  free(self);
  return;
}

// try to decrypt any message sent to us, returns the inner
lob_t e3x_self_decrypt(e3x_self_t self, lob_t message)
{
  e3x_cipher_t cs;
  if(!self || !message) return LOG("bad args");
  if(message->head_len != 1) return LOG("invalid message");
  cs = e3x_cipher_set(message->head[0],NULL);
  if(!cs) return LOG("no cipherset %2x",message->head[0]);
  return cs->local_decrypt(self->locals[cs->id],message);
}

// generate a signature for the data
lob_t e3x_self_sign(e3x_self_t self, lob_t args, uint8_t *data, size_t len)
{
  local_t local = NULL;
  e3x_cipher_t cs = NULL;
  char *alg = lob_get(args,"alg");
  if(!data || !len || !alg) return LOG("bad args");
  cs = e3x_cipher_set(0,alg);
  if(!cs || !cs->local_sign) return LOG("no signing support for %s",alg);
  if(self) local = self->locals[cs->id];
  return cs->local_sign(local,args,data,len);
}
