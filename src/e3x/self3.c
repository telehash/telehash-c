#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "self3.h"
#include "platform.h"

// load secrets/keys to create a new local endpoint
self3_t self3_new(lob_t secrets)
{
  uint8_t i;
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
  }
  
  self->keys = lob_copy(keys);

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
  }

  lob_free(self->keys);
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

