#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "self3.h"
#include "platform.h"
#include "e3x.h"

// load secrets/keys to create a new local endpoint
self3_t self3_new(lob_t secrets)
{
  uint8_t i, ok = 0, hash[32];
  self3_t self;
  lob_t keys = lob_linked(secrets);
  if(!keys) return NULL;

  if(!(self = malloc(sizeof (struct self3_struct)))) return NULL;
  memset(self,0,sizeof (struct self3_struct));

  // give each cset a chance to make a local
  for(i=0; i<CS_MAX; i++)
  {
    if(!cipher3_sets[i] || !cipher3_sets[i]->local_new) continue;
    self->locals[i] = cipher3_sets[i]->local_new(keys, secrets);
    if(!self->locals[i]) continue;
    // make a copy of the binary and encoded keys
    self->keys[i] = lob_get_base32(keys, cipher3_sets[i]->hex);
    lob_set(self->keys[i],"key",lob_get(keys,cipher3_sets[i]->hex));
    // make a hash for the intermediate form for hashnames
    e3x_hash(self->keys[i]->body,self->keys[i]->body_len,hash);
    lob_set_base32(self->keys[i],"hash",hash,32);
    ok++;
  }

  if(!ok)
  {
    self3_free(self);
    return LOG("self failed for %.*s",keys->head_len,keys->head);
  }

  LOG("self created");
  return self;
}

// any exchanges must have been free'd first
void self3_free(self3_t self)
{
  uint8_t i;
  if(!self) return;

  // free any locals created
  for(i=0; i<CS_MAX; i++)
  {
    if(!self->locals[i]) continue;
    cipher3_sets[i]->local_free(self->locals[i]);
    lob_free(self->keys[i]);
  }

  free(self);
  return;
}

// try to decrypt any message sent to us, returns the inner
lob_t self3_decrypt(self3_t self, lob_t message)
{
  cipher3_t cs;
  if(!self || !message) return LOG("bad args");
  if(message->head_len != 1) return LOG("invalid message");
  cs = cipher3_set(message->head[0],NULL);
  if(!cs) return LOG("no cipherset %2x",message->head[0]);
  return cs->local_decrypt(self->locals[cs->id],message);
}

