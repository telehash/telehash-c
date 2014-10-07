#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "sha256.h"
#include "cipher3.h"
#include "platform.h"

// cs1a local ones
#include "uECC.h"
#include "aes.h"
#include "hmac.h"

uint8_t *cs1a_rand(uint8_t *s, uint32_t len);
uint8_t *cs1a_hash(uint8_t *input, uint32_t len, uint8_t *output);
uint8_t *cs1a_err(void);
uint8_t cs1a_generate(lob_t keys, lob_t secrets);

static int RNG(uint8_t *p_dest, unsigned p_size)
{
  cs1a_rand(p_dest,p_size);
  return 1;
}

cipher3_t cs1a_init(lob_t options)
{
  struct timeval tv;
  unsigned int seed;
  cipher3_t ret = malloc(sizeof(struct cipher3_struct));
  if(!ret) return NULL;
  memset(ret,0,sizeof (struct cipher3_struct));

  // normal init stuff
  gettimeofday(&tv, NULL);
  seed = (getpid() << 16) ^ tv.tv_sec ^ tv.tv_usec;
  srandom(seed); // srandomdev() is not universal
  uECC_set_rng(&RNG);

  // configure our callbacks
  ret->rand = cs1a_rand;
  ret->hash = cs1a_hash;
  ret->err = cs1a_err;
  ret->generate = cs1a_generate;
  // TODO

  return ret;
}

uint8_t *cs1a_rand(uint8_t *s, uint32_t len)
{

 uint8_t *x = s;
 
  while(len-- > 0)
  {
    *x = (uint8_t)random();
    x++;
  }
  return s;
}

uint8_t *cs1a_hash(uint8_t *input, uint32_t len, uint8_t *output)
{
  sha256(input,len,output,0);
  return output;
}

uint8_t *cs1a_err(void)
{
  return 0;
}

uint8_t cs1a_generate(lob_t keys, lob_t secrets)
{
  uint8_t id_private[uECC_BYTES], id_public[uECC_BYTES*2];

  if(!uECC_make_key(id_public, id_private)) return 1;
  lob_set_base32(keys,"1a",id_public,uECC_BYTES*2);
  lob_set_base32(secrets,"1a",id_private,uECC_BYTES);

  return 0;
}

