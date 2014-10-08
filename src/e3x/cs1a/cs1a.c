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

// these are all the locally implemented handlers defined in cipher3.h

uint8_t *cs1a_rand(uint8_t *s, uint32_t len);
uint8_t *cs1a_hash(uint8_t *input, uint32_t len, uint8_t *output);
uint8_t *cs1a_err(void);
uint8_t cs1a_generate(lob_t keys, lob_t secrets);

local_t cs1a_local_new(lob_t keys, lob_t secrets);
void cs1a_local_free(local_t local);
lob_t cs1a_local_decrypt(local_t local, lob_t outer);

remote_t cs1a_remote_new(lob_t key);
void cs1a_remote_free(remote_t remote);
uint8_t cs1a_remote_verify(remote_t remote, local_t local, lob_t outer);
lob_t cs1a_remote_encrypt(remote_t remote, local_t local, lob_t inner);

ephemeral_t cs1a_ephemeral_new(remote_t remote, lob_t outer, lob_t inner);
void cs1a_ephemeral_free(ephemeral_t ephemeral);
lob_t cs1a_ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner);
lob_t cs1a_ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer);


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
  
  // identifying markers
  ret->id = CS_1a;
  ret->csid = 0x1a;
  memcpy(ret->hex,"1a",3);

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
  ret->local_new = cs1a_local_new;
  ret->local_free = cs1a_local_free;
  ret->local_decrypt = cs1a_local_decrypt;
  ret->remote_new = cs1a_remote_new;
  ret->remote_free = cs1a_remote_free;
  ret->remote_verify = cs1a_remote_verify;
  ret->remote_encrypt = cs1a_remote_encrypt;
  ret->ephemeral_new = cs1a_ephemeral_new;
  ret->ephemeral_free = cs1a_ephemeral_free;
  ret->ephemeral_encrypt = cs1a_ephemeral_encrypt;
  ret->ephemeral_decrypt = cs1a_ephemeral_decrypt;

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

local_t cs1a_local_new(lob_t keys, lob_t secrets)
{
  return NULL;
}

void cs1a_local_free(local_t local)
{
  return;
}

lob_t cs1a_local_decrypt(local_t local, lob_t outer)
{
  return NULL;
}

remote_t cs1a_remote_new(lob_t key)
{
  return NULL;
}

void cs1a_remote_free(remote_t remote)
{
  
}

uint8_t cs1a_remote_verify(remote_t remote, local_t local, lob_t outer)
{
  return 1;
}

lob_t cs1a_remote_encrypt(remote_t remote, local_t local, lob_t inner)
{
  return NULL;
}

ephemeral_t cs1a_ephemeral_new(remote_t remote, lob_t outer, lob_t inner)
{
  return NULL;
}

void cs1a_ephemeral_free(ephemeral_t ephemeral)
{

}

lob_t cs1a_ephemeral_encrypt(ephemeral_t ephemeral, lob_t inner)
{
  return NULL;
}

lob_t cs1a_ephemeral_decrypt(ephemeral_t ephemeral, lob_t outer)
{
  return NULL;
}
