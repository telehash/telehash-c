#ifndef e3x_self_h
#define e3x_self_h

#include "e3x_cipher.h"

typedef struct e3x_self_struct
{
  lob_t keys[CS_MAX];
  local_t locals[CS_MAX];
} *e3x_self_t;

// load id secrets/keys to create a new local endpoint
e3x_self_t e3x_self_new(lob_t secrets, lob_t keys);
void e3x_self_free(e3x_self_t self); // any exchanges must have been free'd first

// try to decrypt any message sent to us, returns the inner
lob_t e3x_self_decrypt(e3x_self_t self, lob_t message);

// generate a signature for the data
lob_t e3x_self_sign(e3x_self_t self, lob_t args, uint8_t *data, size_t len);

#endif
