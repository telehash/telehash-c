#include "e3x.h"
#include "cipher3.h"
#include "platform.h"
#include <string.h>

// any process-wide initialization
uint8_t e3x_init(lob_t options)
{
  uint8_t err;
  err = cipher3_init(options);
  if(err) return err;
  return 0;
}

// just check every cipher set for any error string
uint8_t *e3x_err(void)
{
  uint8_t i;
  uint8_t *err = NULL;
  for(i=0; i<CS_MAX; i++)
  {
    if(cipher3_sets[i]) err = cipher3_sets[i]->err();
    if(err) return err;
  }
  return err;
}

// generate all the keypairs
lob_t e3x_generate(void)
{
  uint8_t i, err;
  lob_t keys, secrets;
  keys = lob_new();
  secrets = lob_chain(keys);
  for(i=0; i<CS_MAX; i++)
  {
    if(err || !cipher3_sets[i]) continue;
    err = cipher3_sets[i]->generate(keys, secrets);
  }
  if(err) return lob_free(secrets);
  return secrets;
}

// random bytes, from a supported cipher set
uint8_t *e3x_rand(uint8_t *bytes, uint32_t len)
{
  if(!bytes || !len) return bytes;
  if(!cipher3_default)
  {
    DEBUG_PRINTF("e3x not initialized, no cipher_set");
    memset(bytes,0,len);
    return bytes;
  }
  return cipher3_default->rand(bytes, len);
}

// sha256 hashing, from one of the cipher sets
uint8_t *e3x_hash(uint8_t *in, uint32_t len, uint8_t *out32)
{
  if(!in || !len || !out32) return out32;
  if(!cipher3_default)
  {
    DEBUG_PRINTF("e3x not initialized, no cipher_set");
    memset(out32,0,32);
    return out32;
  }
  return cipher3_default->hash(in, len, out32);
}


