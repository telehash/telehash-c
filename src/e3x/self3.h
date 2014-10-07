#ifndef self3_h
#define self3_h

#include "cipher3.h"

typedef struct self3_struct
{
  local_t locals[CS_MAX];
} *self3_t;

// load secrets/keys to create a new local endpoint
self3_t self3_new(lob_t secrets);
void self3_free(self3_t e); // any exchanges must have been free'd first

// try to decrypt any message sent to us, returns the inner
lob_t self3_decrypt(self3_t e, lob_t message);


#endif