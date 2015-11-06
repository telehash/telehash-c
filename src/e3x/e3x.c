#include "telehash.h"
#include "telehash.h"
#include "telehash.h"
#include <string.h>

static uint8_t _initialized = 0;

// any process-wide one-time initialization
uint8_t e3x_init(lob_t options)
{
  uint8_t err;
  if(_initialized) return 0;
  util_sys_random_init();
  err = e3x_cipher_init(options);
  if(err) return err;
  _initialized = 1;
  return 0;
}

// just check every cipher set for any error string
uint8_t *e3x_err(void)
{
  uint8_t i;
  uint8_t *err = NULL;
  for(i=0; i<CS_MAX; i++)
  {
    if(e3x_cipher_sets[i] && e3x_cipher_sets[i]->err) err = e3x_cipher_sets[i]->err();
    if(err) return err;
  }
  return err;
}

// generate all the keypairs
lob_t e3x_generate(void)
{
  uint8_t i, err = 0;
  lob_t keys, secrets;
  keys = lob_new();
  secrets = lob_chain(keys);
  for(err=i=0; i<CS_MAX; i++)
  {
    if(err || !e3x_cipher_sets[i] || !e3x_cipher_sets[i]->generate) continue;
    err = e3x_cipher_sets[i]->generate(keys, secrets);
  }
  if(err) return lob_free(secrets);
  return secrets;
}

// random bytes, from a supported cipher set
uint8_t *e3x_rand(uint8_t *bytes, size_t len)
{
  uint8_t *x = bytes;
  if(!bytes || !len) return bytes;
  if(e3x_cipher_default && e3x_cipher_default->rand) return e3x_cipher_default->rand(bytes, len);

  // crypto lib didn't provide one, use platform's RNG
  while(len-- > 0)
  {
    *x = (uint8_t)util_sys_random();
    x++;
  }
  return bytes;
}

// sha256 hashing, from one of the cipher sets
uint8_t *e3x_hash(uint8_t *in, size_t len, uint8_t *out32)
{
  if(!in || !out32) return out32;
  if(!e3x_cipher_default)
  {
    LOG("e3x not initialized, no cipher_set");
    memset(out32,0,32);
    return out32;
  }
  return e3x_cipher_default->hash(in, len, out32);
}


