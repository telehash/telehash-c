#include "e3x.h"
#include "cipher3.h"

// any process-wide initialization
uint8_t e3x_init(lob_t options)
{
  uint8_t err;
  err = cipher3_init(options);
  if(err) return err;
}

// just check every cipher set for any error string
uint8_t *e3x_err(void)
{
  uint8_t i;
  uint8_t *err;
  for(i=0; i<CS_MAX; i++)
  {
    if(cipher3_sets[i]) err = cipher3_sets[i]->err();
    if(err) return err;
  }
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

