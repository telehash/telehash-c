#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "self3.h"

// load secrets/keys to create a new local endpoint
self3_t self3_new(lob_t secrets)
{
  self3_t self;
  if(!secrets) return NULL;

  if(!(self = malloc(sizeof (struct self3_struct)))) return NULL;
  memset(self,0,sizeof (struct self3_struct));
  // TODO make locals
  return self;
}

// any exchanges must have been free'd first
void self3_free(self3_t self)
{
  if(!self) return;
  // TODO free locals
  free(self);
  return;
}

// try to decrypt any message sent to us, returns the inner
lob_t self3_decrypt(self3_t e, lob_t message)
{
  return NULL;
}

